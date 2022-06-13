/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Mauro I. Dominguez.

==============================================================================*/

#include "vtkSlicerDynamicModelerSelectByPointsTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLModelDisplayNode.h>

// FastMarching includes
#include <vtkFastMarchingGeodesicDistance.h>

// VTK includes
#include <vtkAssignAttribute.h>
#include <vtkCommand.h>
#include <vtkFloatArray.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkGeometryFilter.h>
#include <vtkUnstructuredGrid.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerSelectByPointsTool);

const char* SELECT_BY_POINTS_INPUT_MODEL_REFERENCE_ROLE = "SelectByPoints.InputModel";
const char* SELECT_BY_POINTS_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE = "SelectByPoints.InputFiducial";
const char* SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECT_BY_POINTS_SCALARS_REFERENCE_ROLE = "SelectByPoints.SelectionScalarsModel";
const char* SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE = "SelectByPoints.SelectedFacesModel";

const char* SELECTION_ARRAY_NAME = "Selection";
const char* DISTANCE_ARRAY_NAME = "Distance";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerSelectByPointsTool::vtkSlicerDynamicModelerSelectByPointsTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextValue(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextValue(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextValue(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo inputModel(
    "Model node",
    "Model node to select faces from.",
    inputModelClassNames,
    SELECT_BY_POINTS_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputFiducialListEvents;
  inputFiducialListEvents->InsertNextValue(vtkCommand::ModifiedEvent);
  inputFiducialListEvents->InsertNextValue(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputFiducialListEvents->InsertNextValue(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputFiducialListClassNames;
  inputFiducialListClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  NodeInfo inputFiducialList(
    "Fiducials node",
    "Fiducials node to make the selection of model's faces.",
    inputFiducialListClassNames,
    SELECT_BY_POINTS_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE,
    true,
    false,
    inputFiducialListEvents
    );
  this->InputNodeInfo.push_back(inputFiducialList);

  /////////
  // Outputs
  NodeInfo outputSelectionScalarsModel(
    "Model with selection scalars",
    "All model points have a selected scalar value that is 0 or 1.",
    inputModelClassNames,
    SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECT_BY_POINTS_SCALARS_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputSelectionScalarsModel);

  NodeInfo outputSelectedFacesModel(
    "Model of the selected cells.",
    "Model that only contains the selected faces of the input model.",
    inputModelClassNames,
    SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputSelectedFacesModel);

  /////////
  // Parameters
  ParameterInfo parameterSelectionDistance(
    "Selection distance",
    "Selection distance of model's points to input fiducials.",
    "SelectionDistance",
    PARAMETER_DOUBLE,
    5.0);
  this->InputParameterInfo.push_back(parameterSelectionDistance);

  ParameterInfo parameterSelectionAlgorithm(
    "Selection algorithm",
    "Method used to calculate points distance to seeds."
      " SphereRadius method uses straight line distance. GeodesicDistance method uses distance on surface.",
    "SelectionAlgorithm",
    PARAMETER_STRING_ENUM,
    "SphereRadius");

  vtkNew<vtkStringArray> possibleValues;
  parameterSelectionAlgorithm.PossibleValues = possibleValues;
  parameterSelectionAlgorithm.PossibleValues->InsertNextValue("SphereRadius");
  parameterSelectionAlgorithm.PossibleValues->InsertNextValue("GeodesicDistance");
  this->InputParameterInfo.push_back(parameterSelectionAlgorithm);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->SelectionScalarsOutputMesh = vtkSmartPointer<vtkPolyData>::New();
  this->SelectedFacesOutputMesh = vtkSmartPointer<vtkPolyData>::New();

  this->InputMeshLocator_World = vtkSmartPointer<vtkPointLocator>::New();

  this->GeodesicDistance = vtkSmartPointer<vtkFastMarchingGeodesicDistance>::New();

  this->OutputSelectionScalarsModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputSelectionScalarsModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputSelectionScalarsModelTransformFilter->SetTransform(this->OutputSelectionScalarsModelTransform);

  this->OutputSelectedFacesModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputSelectedFacesModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputSelectedFacesModelTransformFilter->SetTransform(this->OutputSelectedFacesModelTransform);

  this->SelectionArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
  this->SelectionArray->SetName(SELECTION_ARRAY_NAME);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerSelectByPointsTool::~vtkSlicerDynamicModelerSelectByPointsTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerSelectByPointsTool::GetName()
{
  return "Select by points";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerSelectByPointsTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputSelectionScalarsModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECT_BY_POINTS_SCALARS_REFERENCE_ROLE));
  vtkMRMLModelNode* outputSelectedFacesModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE));
  if (!outputSelectionScalarsModelNode && !outputSelectedFacesModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLNode* inputNode = surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE);
  vtkMRMLMarkupsFiducialNode* fiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(inputNode);
  if (!fiducialNode)
    {
    vtkErrorMacro("Invalid input fiducial node!");
    return false;
    }
  if (fiducialNode->GetNumberOfControlPoints() == 0)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_INPUT_MODEL_REFERENCE_ROLE));
  if (!inputModelNode)
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
    return true;
    }

  // Set inputMesh_World
  vtkSmartPointer<vtkPolyData> inputMesh_World;
  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelNodeToWorldTransform);
    this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());
    this->InputModelToWorldTransformFilter->Update();
    inputMesh_World = this->InputModelToWorldTransformFilter->GetOutput();
    }
  else
    {
    // Not transformed
    // Deallocate transform object (it may be a large composite transform)
    this->InputModelNodeToWorldTransform->Identity();
    // Directly use the input model node's polydata, as this allows reusing the previously initialized locator
    inputMesh_World = vtkPolyData::SafeDownCast(inputModelNode->GetMesh());
    }
  
  // Initialize the locator
  this->InputMeshLocator_World->SetDataSet(inputMesh_World);

  double selectionDistance = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();

  std::string selectionAlgorithm = this->GetNthInputParameterValue(1, surfaceEditorNode).ToString();

  bool computeSelectionScalarsModel = (outputSelectionScalarsModelNode != nullptr);
  bool computeSelectedFacesModel = (outputSelectedFacesModelNode != nullptr);

  bool success = false;
  vtkSmartPointer<vtkPolyData> selectedFacesMesh_World;
  if (selectionAlgorithm == "SphereRadius")
    {
    success = this->UpdateUsingSphereRadius(inputMesh_World, fiducialNode, selectionDistance,
      computeSelectionScalarsModel, computeSelectedFacesModel, this->SelectionArray, selectedFacesMesh_World);
    }
  else
    {
    success = this->UpdateUsingGeodesicDistance(inputMesh_World, fiducialNode, selectionDistance,
      computeSelectionScalarsModel, computeSelectedFacesModel, this->SelectionArray, selectedFacesMesh_World);
    }
  if (!success)
    {
    return false;
    }

  if (computeSelectionScalarsModel)
    {
    // Get an independent copy of the input mesh in node coordinate system
    if (outputSelectionScalarsModelNode->GetParentTransformNode())
      {
      outputSelectionScalarsModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputSelectionScalarsModelTransform);
      this->OutputSelectionScalarsModelTransformFilter->SetInputData(inputMesh_World);
      this->OutputSelectionScalarsModelTransformFilter->Update();
      this->SelectionScalarsOutputMesh->DeepCopy(this->OutputSelectionScalarsModelTransformFilter->GetOutput());
      }
    else
      {
      // Not transformed
      this->SelectionScalarsOutputMesh->DeepCopy(inputMesh_World);
      }

    vtkPointData* pointScalars = vtkPointData::SafeDownCast(this->SelectionScalarsOutputMesh->GetPointData());
    pointScalars->AddArray(this->SelectionArray);
    
    outputSelectionScalarsModelNode->SetAndObservePolyData(this->SelectionScalarsOutputMesh);
    outputSelectionScalarsModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  if (computeSelectedFacesModel)
    {
    // Get clipped output mesh
    if (outputSelectedFacesModelNode->GetParentTransformNode())
      {
      outputSelectedFacesModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputSelectedFacesModelTransform);
      this->OutputSelectedFacesModelTransformFilter->SetInputData(selectedFacesMesh_World);
      this->OutputSelectedFacesModelTransformFilter->Update();
      this->SelectedFacesOutputMesh->DeepCopy(this->OutputSelectedFacesModelTransformFilter->GetOutput());
      }
    else
      {
      // Not transformed
      this->SelectedFacesOutputMesh->DeepCopy(selectedFacesMesh_World);
      }
  
    MRMLNodeModifyBlocker blocker(outputSelectedFacesModelNode);
    outputSelectedFacesModelNode->SetAndObserveMesh(this->SelectedFacesOutputMesh);
    outputSelectedFacesModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

    return true;
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerSelectByPointsTool::CreateOutputDisplayNodes(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  // Set up coloring by selection array
  vtkMRMLModelNode* outputSelectionScalarsModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECT_BY_POINTS_SCALARS_REFERENCE_ROLE));
  if (outputSelectionScalarsModelNode && !outputSelectionScalarsModelNode->GetModelDisplayNode())
    {
    outputSelectionScalarsModelNode->CreateDefaultDisplayNodes();
    vtkMRMLModelDisplayNode* outputDisplayNode = outputSelectionScalarsModelNode->GetModelDisplayNode();
    outputDisplayNode->SetActiveScalar(SELECTION_ARRAY_NAME, vtkAssignAttribute::POINT_DATA);
    outputDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeFileViridis.txt");
    outputDisplayNode->SetScalarVisibility(true);
    }

  // Set up coloring by copying from input
  vtkMRMLModelNode* outputSelectedFacesModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE));
  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECT_BY_POINTS_INPUT_MODEL_REFERENCE_ROLE));
  if (inputModelNode && outputSelectedFacesModelNode && !outputSelectedFacesModelNode->GetModelDisplayNode())
    {
    outputSelectedFacesModelNode->CreateDefaultDisplayNodes();
    vtkMRMLModelDisplayNode* outputDisplayNode = outputSelectedFacesModelNode->GetModelDisplayNode();
    vtkMRMLModelDisplayNode* inputDisplayNode = inputModelNode->GetModelDisplayNode();
    outputDisplayNode->SetColor(inputDisplayNode->GetColor());
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerSelectByPointsTool::UpdateUsingSphereRadius(vtkPolyData* inputMesh_World, vtkMRMLMarkupsFiducialNode* fiducialNode,
  double selectionDistance, bool vtkNotUsed(computeSelectionScalarsModel), bool computeSelectedFacesModel,
  vtkUnsignedCharArray* outputSelectionArray, vtkSmartPointer<vtkPolyData>& selectedFacesMesh_World)
{
  // Update outputSelectionArray
  outputSelectionArray->SetNumberOfValues(inputMesh_World->GetNumberOfPoints());
  outputSelectionArray->Fill(0);
  for (int fiducialIndex = 0; fiducialIndex < fiducialNode->GetNumberOfControlPoints(); fiducialIndex++)
    {
    double position[3] = { 0.0, 0.0, 0.0 };
    fiducialNode->GetNthControlPointPositionWorld(fiducialIndex,position);
    vtkNew<vtkIdList> pointIdsWithinRadius;
    this->InputMeshLocator_World->FindPointsWithinRadius(selectionDistance, position, pointIdsWithinRadius);
    const vtkIdType numberOfPointIdsWithinRadius = pointIdsWithinRadius->GetNumberOfIds();
    for (vtkIdType pointIdIndex = 0; pointIdIndex < numberOfPointIdsWithinRadius; pointIdIndex++)
      {
      outputSelectionArray->SetValue(pointIdsWithinRadius->GetId(pointIdIndex), 1);
      }
    }

  if (computeSelectedFacesModel)
    {
    // Create shallow copy of the input model (in world coordinate system) that will contain selection scalars
    vtkNew<vtkPolyData> inputMesh_WorldWithSelection;
    inputMesh_WorldWithSelection->ShallowCopy(inputMesh_World);

    vtkPointData* pointScalars = vtkPointData::SafeDownCast(inputMesh_WorldWithSelection->GetPointData());
    pointScalars->AddArray(outputSelectionArray);

    vtkNew<vtkThreshold> thresholdFilter;
    thresholdFilter->SetInputData(inputMesh_WorldWithSelection);
    thresholdFilter->SetUpperThreshold(0.5);
    thresholdFilter->SetThresholdFunction(vtkThreshold::THRESHOLD_UPPER);
    thresholdFilter->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, SELECTION_ARRAY_NAME);
    thresholdFilter->Update();
    vtkNew<vtkGeometryFilter> geometryFilter;
    geometryFilter->SetInputData(vtkUnstructuredGrid::SafeDownCast(thresholdFilter->GetOutput()));
    geometryFilter->Update();

    selectedFacesMesh_World = geometryFilter->GetOutput();
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerSelectByPointsTool::UpdateUsingGeodesicDistance(vtkPolyData* inputMesh_World, vtkMRMLMarkupsFiducialNode* fiducialNode,
  double selectionDistance, bool computeSelectionScalarsModel, bool computeSelectedFacesModel,
  vtkUnsignedCharArray* outputSelectionArray, vtkSmartPointer<vtkPolyData>& selectedFacesMesh_World)
{
  vtkNew<vtkIdList> seeds;
  for (int fiducialIndex = 0; fiducialIndex < fiducialNode->GetNumberOfControlPoints(); fiducialIndex++)
    {
    double position[3] = { 0.0, 0.0, 0.0 };
    fiducialNode->GetNthControlPointPositionWorld(fiducialIndex, position);
    vtkIdType pointIDOfClosestPoint = this->InputMeshLocator_World->FindClosestPoint(position);
    seeds->InsertNextId(pointIDOfClosestPoint);
    }

  this->GeodesicDistance->SetInputData(inputMesh_World);
  this->GeodesicDistance->SetFieldDataName(DISTANCE_ARRAY_NAME);
  this->GeodesicDistance->SetSeeds(seeds.GetPointer());
  this->GeodesicDistance->SetDistanceStopCriterion(selectionDistance);
  this->GeodesicDistance->Update();

  if (computeSelectionScalarsModel)
    {
    vtkPointData* pointScalars = vtkPointData::SafeDownCast(this->GeodesicDistance->GetOutput()->GetPointData());
    vtkFloatArray* distanceArray = vtkFloatArray::SafeDownCast(pointScalars->GetArray(DISTANCE_ARRAY_NAME));

    vtkIdType numberOfPoints = this->GeodesicDistance->GetOutput()->GetNumberOfPoints();
    outputSelectionArray->SetNumberOfValues(numberOfPoints);
    outputSelectionArray->Fill(0);

    for (int i = 0; i < numberOfPoints; i++)
      {
      if ((distanceArray->GetValue(i) < selectionDistance) && (distanceArray->GetValue(i) > -1e-5))
        {
        outputSelectionArray->SetValue(i, 1);
        }
      }
    }

  if (computeSelectedFacesModel)
    {
    vtkNew<vtkThreshold> thresholdFilter;
    thresholdFilter->SetInputData(this->GeodesicDistance->GetOutput());
    thresholdFilter->ThresholdBetween(-1e-5, selectionDistance);
    thresholdFilter->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, DISTANCE_ARRAY_NAME);

    vtkNew<vtkGeometryFilter> geometryFilter;
    geometryFilter->SetInputConnection(thresholdFilter->GetOutputPort());
    geometryFilter->Update();

    selectedFacesMesh_World = geometryFilter->GetOutput();
    }

  return true;
}
