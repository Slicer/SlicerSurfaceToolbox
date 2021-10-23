/*==============================================================================

==============================================================================*/

#include "vtkSlicerDynamicModelerSelectionTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkClipPolyData.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkContourTriangulator.h>
#include <vtkCutter.h>
#include <vtkDataSetAttributes.h>
#include <vtkFeatureEdges.h>
#include <vtkFloatArray.h>
#include <vtkGeneralTransform.h>
#include <vtkImplicitBoolean.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkPolygon.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkReverseSense.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStripper.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkFastMarchingGeodesicDistance.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerSelectionTool);

const char* SELECTION_INPUT_MODEL_REFERENCE_ROLE = "Selection.InputModel";
const char* SELECTION_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE = "Selection.InputFiducial";
const char* SELECTION_OUTPUT_MODEL_WITH_SELECTION_SCALARS_REFERENCE_ROLE = "Selection.SelectionScalarsModel";
const char* SELECTION_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE = "Selection.SelectedFacesModel";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerSelectionTool::vtkSlicerDynamicModelerSelectionTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo inputModel(
    "Model node",
    "Model node to select faces from.",
    inputModelClassNames,
    SELECTION_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputFiducialListEvents;
  inputFiducialListEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputFiducialListEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputFiducialListEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputFiducialListClassNames;
  inputFiducialListClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  NodeInfo inputFiducialList(
    "Fiducials node",
    "Fiducials node to make the selection of model's faces.",
    inputFiducialListClassNames,
    SELECTION_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE,
    true,
    false,
    inputFiducialListEvents
    );
  this->InputNodeInfo.push_back(inputFiducialList);

  /////////
  // Outputs
  NodeInfo outputSelectionScalarsModel(
    "Model with selection scalars",
    "All model cells have a selected scalar value that is 0 or 1.",
    inputModelClassNames,
    SELECTION_OUTPUT_MODEL_WITH_SELECTION_SCALARS_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputSelectionScalarsModel);

  NodeInfo outputSelectedFacesModel(
    "Model of the selected cells.",
    "Model that only contains the selected faces of the input model.",
    inputModelClassNames,
    SELECTION_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE,
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
  "Method used to calculate points distance to seeds.",
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

  this->OutputSelectionScalarsModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputSelectionScalarsModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputSelectionScalarsModelTransformFilter->SetTransform(this->OutputSelectionScalarsModelTransform);

  this->OutputSelectedFacesModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputSelectedFacesModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputSelectedFacesModelTransformFilter->SetTransform(this->OutputSelectedFacesModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerSelectionTool::~vtkSlicerDynamicModelerSelectionTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerSelectionTool::GetName()
{
  return "Selection";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerSelectionTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputSelectionScalarsModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECTION_OUTPUT_MODEL_WITH_SELECTION_SCALARS_REFERENCE_ROLE));
  vtkMRMLModelNode* outputSelectedFacesModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECTION_OUTPUT_MODEL_WITH_SELECTED_FACES_REFERENCE_ROLE));
  if (!outputSelectionScalarsModelNode && !outputSelectedFacesModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLNode* inputNode = surfaceEditorNode->GetNodeReference(SELECTION_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE);
  vtkMRMLMarkupsFiducialNode* fiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(inputNode);
  if (!fiducialNode)
    {
    vtkErrorMacro("Invalid input fiducial node!");
    return false;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SELECTION_INPUT_MODEL_REFERENCE_ROLE));
  if (!inputModelNode)
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
    return true;
    }

  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelNodeToWorldTransform);
    }
  else
    {
    this->InputModelNodeToWorldTransform->Identity();
    }
  if (outputSelectionScalarsModelNode && outputSelectionScalarsModelNode->GetParentTransformNode())
    {
    outputSelectionScalarsModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputSelectionScalarsModelTransform);
    }
  if (outputSelectedFacesModelNode && outputSelectedFacesModelNode->GetParentTransformNode())
    {
    outputSelectedFacesModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputSelectedFacesModelTransform);
    }

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());

  double selectionDistance = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();

  std::string selectionAlgorithm = this->GetNthInputParameterValue(1, surfaceEditorNode).ToString();
  if (selectionAlgorithm == "SphereRadius")
    {
    vtkNew<vtkPolyData> outputMesh;
    outputMesh->ShallowCopy(this->InputModelToWorldTransformFilter->GetOutput());

    # Initialize the locator
    vtkNew<vtkPointLocator> pointTree;
    pointTree->SetDataSet(outputMesh);
    pointTree->BuildLocator();

    vtkNew<vtkIntArray> selectionArray;
    selectionArray->SetName("Selection");
    selectionArray->SetNumberOfValues(outputMesh->GetNumberOfPoints());
    selectionArray->Fill(0);

    for (int i = 0; i < fiducialNode->GetNumberOfControlPoints(); i++)
      {
      position = [0,0,0];
      fiducialList->GetNthControlPointPositionWorld(i,position);
      vtkNew<vtkIdList> result;
      pointTree->FindPointsWithinRadius(selectionDistance, position,
                                      result);
      
      for (int j = 0; i < result->GetNumberOfIds(); i++)
        {
        point_ind = result->GetId(j);
        selectionArray->SetTuple1(point_ind, 1);
        }
      }

    vtkPointData *pointScalars = vtkPointData::SafeDownCast(modelPolydata->GetPointData());
    pointScalars->AddArray(selectionArray);

    if (outputSelectionScalarsModelNode)
      {
      //Do selectionScalarsModelProcessing
      this->OutputSelectionScalarsModelTransformFilter->SetInputData(outputMesh);
      this->OutputSelectionScalarsModelTransformFilter->Update();
      outputMesh->DeepCopy(this->OutputSelectionScalarsModelTransformFilter->GetOutput());

      MRMLNodeModifyBlocker blocker(outputSelectionScalarsModelNode);
      outputSelectionScalarsModelNode->SetAndObserveMesh(outputMesh);
      outputSelectionScalarsModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

      # Set up coloring by selection array
      vtkMRMLModelDisplayNode* outputDisplayNode = outputSelectionScalarsModelNode->GetDisplayNode();
      outputDisplayNode->SetActiveScalar("Selection", vtk::vtkAssignAttribute::POINT_DATA);
      //outputDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeWarm1");
      outputDisplayNode->SetScalarVisibility(true);
      }

    if (outputSelectedFacesModelNode)
      {
      //Do selectedFacesModelProcessing

      vtkNew<vtkThreshold> thresholdFilter;
      thresholdFilter->SetInputData(outputMesh);
      thresholdFilter->ThresholdBetween(0.9, 1.1);
      thresholdFilter->SetInputArrayToProcess(0, 0, 0,
          vtk::vtkDataObject::FIELD_ASSOCIATION_POINTS,
          "Selection");
      thresholdFilter->Update();
      vtkNew<vtkGeometryFilter> geometryFilter;
      geometryFilter->SetInputData(thresholdFilter->GetOutput());
      geometryFilter->Update();

      this->OutputSelectedFacesModelTransformFilter->SetInputData(geometryFilter->GetOutput());
      this->OutputSelectedFacesModelTransformFilter->Update();
      outputMesh->DeepCopy(this->OutputSelectedFacesModelTransformFilter->GetOutput());

      MRMLNodeModifyBlocker blocker(outputSelectedFacesModelNode);
      outputSelectedFacesModelNode->SetAndObserveMesh(outputMesh);
      outputSelectedFacesModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
      }
    }
  else
    {
      # Initialize the locator
      vtkNew<vtkPointLocator> pointTree;
      pointTree->SetDataSet(outputMesh);
      pointTree->BuildLocator();

      vtkNew<vtkIdList> seeds;
      
      for (int i = 0; i < fiducialNode->GetNumberOfControlPoints(); i++)
        {
        position = [0,0,0];
        fiducialList->GetNthControlPointPositionWorld(i,position);
        vtkIdType pointIDOfClosestPoint = pointTree->FindClosestPoint(position)
        seeds->InsertNextId(pointIDOfClosestPoint);

      vtkNew<vtkFastMarchingGeodesicDistance> Geodesic;
      Geodesic->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
      Geodesic->SetFieldDataName("FMMDist");
      Geodesic->SetSeeds(seeds.GetPointer());
      Geodesic->SetDistanceStopCriterion(selectionDistance);
      Geodesic->Update();

      if (outputSelectionScalarsModelNode)
        {
        vtkNew<vtkPolyData> outputMesh;
        outputMesh->ShallowCopy(Geodesic->GetOutput());

        vtkPointData *pointScalars = vtkPointData::SafeDownCast(outputMesh->GetPointData());
        vtkFloatArray *distanceArray = pointScalars.GetArray("FMMDist")
        vtkNew<vtkIntArray> selectionArray;
        selectionArray->SetName("Selection");
        selectionArray->SetNumberOfValues(outputMesh.GetNumberOfPoints());
        selectionArray->Fill(0);
        for (int i = 0; i < outputMesh->GetNumberOfPoints(); i++)
          {
          if (distanceArray->GetTuple1(i) < selectionDistance)
            {
            selectionArray->SetTuple1(i,1)
            }
          }

        pointScalars->AddArray(selectionArray)

        //Do selectionScalarsModelProcessing
        this->OutputSelectionScalarsModelTransformFilter->SetInputData(outputMesh);
        this->OutputSelectionScalarsModelTransformFilter->Update();
        outputMesh->DeepCopy(this->OutputSelectionScalarsModelTransformFilter->GetOutput());

        MRMLNodeModifyBlocker blocker(outputSelectionScalarsModelNode);
        outputSelectionScalarsModelNode->SetAndObserveMesh(outputMesh);
        outputSelectionScalarsModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

        # Set up coloring by selection array
        vtkMRMLModelDisplayNode* outputDisplayNode = outputSelectionScalarsModelNode->GetDisplayNode();
        outputDisplayNode->SetActiveScalar("Selection", vtk::vtkAssignAttribute::POINT_DATA);
        //outputDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeWarm1");
        outputDisplayNode->SetScalarVisibility(true);
        }
      if (outputSelectedFacesModelNode)
        {
        //Do selectedFacesModelProcessing
        vtkNew<vtkThreshold> thresholdFilter;
        thresholdFilter->SetInputData(Geodesic->GetOutput());
        thresholdFilter->ThresholdBetween(0, selectionDistance);
        thresholdFilter->SetInputArrayToProcess(0, 0, 0,
            vtk::vtkDataObject::FIELD_ASSOCIATION_POINTS,
            "FMMDist");
        thresholdFilter->Update();
        vtkNew<vtkGeometryFilter> geometryFilter;
        geometryFilter->SetInputData(thresholdFilter->GetOutput());
        geometryFilter->Update();

        this->OutputSelectedFacesModelTransformFilter->SetInputData(geometryFilter->GetOutput());
        this->OutputSelectedFacesModelTransformFilter->Update();
        outputMesh->DeepCopy(this->OutputSelectedFacesModelTransformFilter->GetOutput());

        MRMLNodeModifyBlocker blocker(outputSelectedFacesModelNode);
        outputSelectedFacesModelNode->SetAndObserveMesh(outputMesh);
        outputSelectedFacesModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
        }
    }

  

  return true;
}
