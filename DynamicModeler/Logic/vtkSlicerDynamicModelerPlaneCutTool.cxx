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

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, Cancer
  Care Ontario, OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.

==============================================================================*/

#include "vtkSlicerDynamicModelerPlaneCutTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkClipPolyData.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkDataObject.h>
#include <vtkGeneralTransform.h>
#include <vtkGeometryFilter.h>
#include <vtkImplicitBoolean.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkReverseSense.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerPlaneCutTool);

const char* PLANE_CUT_INPUT_MODEL_REFERENCE_ROLE = "PlaneCut.InputModel";
const char* PLANE_CUT_INPUT_PLANE_REFERENCE_ROLE = "PlaneCut.InputPlane";
const char* PLANE_CUT_OUTPUT_POSITIVE_MODEL_REFERENCE_ROLE = "PlaneCut.OutputPositiveModel";
const char* PLANE_CUT_OUTPUT_NEGATIVE_MODEL_REFERENCE_ROLE = "PlaneCut.OutputNegativeModel";

namespace
{
bool IntersectionOutputIsPositive(int operationType)
{
  return operationType == vtkImplicitBoolean::VTK_UNION;
}

void CreateIntersectionPlanes(vtkPlaneCollection* planes, int operationType, vtkPlaneCollection* intersectionPlanes)
{
  for (int planeIndex = 0; planeIndex < planes->GetNumberOfItems(); ++planeIndex)
    {
    vtkPlane* sourcePlane = planes->GetItem(planeIndex);
    double origin[3] = { 0.0 };
    double normal[3] = { 0.0 };
    sourcePlane->GetOrigin(origin);
    sourcePlane->GetNormal(normal);

    bool keepNegativeSide = operationType == vtkImplicitBoolean::VTK_INTERSECTION
      || (operationType == vtkImplicitBoolean::VTK_DIFFERENCE && planeIndex == 0);
    if (keepNegativeSide)
      {
      vtkMath::MultiplyScalar(normal, -1.0);
      }

    vtkNew<vtkPlane> intersectionPlane;
    intersectionPlane->SetOrigin(origin);
    intersectionPlane->SetNormal(normal);
    intersectionPlanes->AddItem(intersectionPlane);
    }
}

void AppendEndCap(vtkPolyData* clippedPolyData, vtkPolyData* endCapPolyData, vtkPolyData* outputPolyData)
{
  vtkNew<vtkAppendPolyData> appendFilter;
  appendFilter->AddInputData(clippedPolyData);
  appendFilter->AddInputData(endCapPolyData);
  appendFilter->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);

  vtkNew<vtkCleanPolyData> cleanFilter;
  cleanFilter->SetInputConnection(appendFilter->GetOutputPort());
  cleanFilter->PointMergingOn();
  cleanFilter->ToleranceIsAbsoluteOn();
  cleanFilter->SetAbsoluteTolerance(0.0);
  cleanFilter->ConvertLinesToPointsOff();
  cleanFilter->ConvertPolysToLinesOff();
  cleanFilter->ConvertStripsToPolysOff();
  cleanFilter->Update();
  outputPolyData->ShallowCopy(cleanFilter->GetOutput());
}

void SplitPolyDataByPlanes(vtkPlaneCollection* planes, int firstPlaneIndex,
  vtkPolyData* inputPolyData, vtkPolyData* outputPolyData)
{
  outputPolyData->ShallowCopy(inputPolyData);
  for (int planeIndex = firstPlaneIndex; planeIndex < planes->GetNumberOfItems(); ++planeIndex)
    {
    vtkNew<vtkClipPolyData> splitter;
    splitter->SetInputData(outputPolyData);
    splitter->SetClipFunction(planes->GetItem(planeIndex));
    splitter->SetValue(0.0);
    splitter->GenerateClippedOutputOn();
    splitter->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);

    vtkNew<vtkAppendPolyData> appendFilter;
    appendFilter->AddInputConnection(splitter->GetOutputPort());
    appendFilter->AddInputConnection(splitter->GetClippedOutputPort());
    appendFilter->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);

    vtkNew<vtkCleanPolyData> cleanFilter;
    cleanFilter->SetInputConnection(appendFilter->GetOutputPort());
    cleanFilter->PointMergingOn();
    cleanFilter->ToleranceIsAbsoluteOn();
    cleanFilter->SetAbsoluteTolerance(0.0);
    cleanFilter->ConvertLinesToPointsOff();
    cleanFilter->ConvertPolysToLinesOff();
    cleanFilter->ConvertStripsToPolysOff();
    cleanFilter->Update();
    outputPolyData->ShallowCopy(cleanFilter->GetOutput());
    }
}

void CreateCappedOutputs(vtkPlaneCollection* planes, vtkPolyData* inputPolyData, int operationType,
  vtkPolyData* positiveOutput, vtkPolyData* negativeOutput)
{
  vtkNew<vtkPlaneCollection> intersectionPlanes;
  CreateIntersectionPlanes(planes, operationType, intersectionPlanes);

  vtkNew<vtkClipClosedSurface> intersectionClipper;
  intersectionClipper->SetInputData(inputPolyData);
  intersectionClipper->SetClippingPlanes(intersectionPlanes);
  intersectionClipper->GenerateClipFaceOutputOn();
  intersectionClipper->Update();

  vtkNew<vtkAppendPolyData> complementBodyAppendFilter;
  for (int planeIndex = 0; planeIndex < intersectionPlanes->GetNumberOfItems(); ++planeIndex)
    {
    vtkNew<vtkPlaneCollection> complementPiecePlanes;
    for (int previousPlaneIndex = 0; previousPlaneIndex < planeIndex; ++previousPlaneIndex)
      {
      complementPiecePlanes->AddItem(intersectionPlanes->GetItem(previousPlaneIndex));
      }

    vtkPlane* sourcePlane = intersectionPlanes->GetItem(planeIndex);
    double origin[3] = { 0.0 };
    double normal[3] = { 0.0 };
    sourcePlane->GetOrigin(origin);
    sourcePlane->GetNormal(normal);
    vtkMath::MultiplyScalar(normal, -1.0);

    vtkNew<vtkPlane> complementPlane;
    complementPlane->SetOrigin(origin);
    complementPlane->SetNormal(normal);
    complementPiecePlanes->AddItem(complementPlane);

    vtkNew<vtkClipClosedSurface> complementPieceClipper;
    complementPieceClipper->SetInputData(inputPolyData);
    complementPieceClipper->SetClippingPlanes(complementPiecePlanes);
    complementPieceClipper->SetScalarModeToLabels();

    // vtkClipClosedSurface labels original cells as 0 and generated cut faces as 1 or 2.
    vtkNew<vtkThreshold> originalSurfaceThreshold;
    originalSurfaceThreshold->SetInputConnection(complementPieceClipper->GetOutputPort());
    originalSurfaceThreshold->SetInputArrayToProcess(
      0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "Labels");
    originalSurfaceThreshold->SetLowerThreshold(0.0);
    originalSurfaceThreshold->SetUpperThreshold(0.0);
    originalSurfaceThreshold->SetThresholdFunction(vtkThreshold::THRESHOLD_BETWEEN);

    vtkNew<vtkGeometryFilter> originalSurfaceFilter;
    originalSurfaceFilter->SetInputConnection(originalSurfaceThreshold->GetOutputPort());
    originalSurfaceFilter->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);
    originalSurfaceFilter->Update();

    vtkNew<vtkPolyData> conformingOriginalSurface;
    SplitPolyDataByPlanes(intersectionPlanes, planeIndex + 1,
      originalSurfaceFilter->GetOutput(), conformingOriginalSurface);
    complementBodyAppendFilter->AddInputData(conformingOriginalSurface);
    }
  complementBodyAppendFilter->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);
  complementBodyAppendFilter->Update();

  vtkNew<vtkReverseSense> reverseCap;
  reverseCap->SetInputData(intersectionClipper->GetClipFaceOutput());
  reverseCap->ReverseCellsOn();
  reverseCap->ReverseNormalsOn();
  reverseCap->Update();

  vtkNew<vtkPolyData> complementOutput;
  AppendEndCap(complementBodyAppendFilter->GetOutput(), reverseCap->GetOutput(), complementOutput);

  if (IntersectionOutputIsPositive(operationType))
    {
    positiveOutput->ShallowCopy(intersectionClipper->GetOutput());
    negativeOutput->ShallowCopy(complementOutput);
    }
  else
    {
    positiveOutput->ShallowCopy(complementOutput);
    negativeOutput->ShallowCopy(intersectionClipper->GetOutput());
    }
}
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerPlaneCutTool::vtkSlicerDynamicModelerPlaneCutTool()
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
    "Model node to be cut.",
    inputModelClassNames,
    PLANE_CUT_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputPlaneEvents;
  inputPlaneEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputPlaneEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputPlaneEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputPlaneClassNames;
  inputPlaneClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  inputPlaneClassNames->InsertNextValue("vtkMRMLSliceNode");
  NodeInfo inputPlane(
    "Plane node",
    "Plane node to cut the model node.",
    inputPlaneClassNames,
    PLANE_CUT_INPUT_PLANE_REFERENCE_ROLE,
    true,
    true,
    inputPlaneEvents
    );
  this->InputNodeInfo.push_back(inputPlane);

  /////////
  // Outputs
  NodeInfo outputNegativeModel(
    "Output model (negative side)",
    "Portion of the cut model that is on the opposite side of the plane as the normal.",
    inputModelClassNames,
    PLANE_CUT_OUTPUT_NEGATIVE_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputNegativeModel);

  NodeInfo outputPositiveModel(
    "Complement output model (positive side)",
    "Portion of the cut model that is on the same side of the plane as the normal.",
    inputModelClassNames,
    PLANE_CUT_OUTPUT_POSITIVE_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputPositiveModel);

  /////////
  // Parameters
  ParameterInfo parameterCapSurface(
    "Cap surface",
    "Create a closed surface by triangulating the clipped region",
    "CapSurface",
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterCapSurface);

  ParameterInfo parameterOperationType(
    "Operation type",
    "Method used for combining the negative sides of the planes. This setting has no effect if only a single clipping plane is selected.",
    "OperationType",
    PARAMETER_STRING_ENUM,
    "Union");

  vtkNew<vtkStringArray> possibleValues;
  parameterOperationType.PossibleValues = possibleValues;
  parameterOperationType.PossibleValues->InsertNextValue("Union");
  parameterOperationType.PossibleValues->InsertNextValue("Intersection");
  parameterOperationType.PossibleValues->InsertNextValue("Difference");
  this->InputParameterInfo.push_back(parameterOperationType);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);
  this->InputModelToWorldTransformFilter->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);

  this->PlaneClipper = vtkSmartPointer<vtkClipPolyData>::New();
  this->PlaneClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->PlaneClipper->SetValue(0.0);
  this->PlaneClipper->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);

  // vtkClipPolyData leaves points in the output that are not used in any cells.
  // Set up cleaner filter to remove those (and do nothing else).

  this->OutputPositiveCleanFilter = vtkSmartPointer<vtkCleanPolyData>::New();
  this->OutputPositiveCleanFilter->SetInputConnection(this->PlaneClipper->GetOutputPort());
  this->OutputPositiveCleanFilter->PointMergingOff();
  this->OutputPositiveCleanFilter->ConvertLinesToPointsOff();
  this->OutputPositiveCleanFilter->ConvertPolysToLinesOff();
  this->OutputPositiveCleanFilter->ConvertStripsToPolysOff();

  this->OutputNegativeCleanFilter = vtkSmartPointer<vtkCleanPolyData>::New();
  this->OutputNegativeCleanFilter->SetInputConnection(this->PlaneClipper->GetClippedOutputPort());
  this->OutputNegativeCleanFilter->PointMergingOff();
  this->OutputNegativeCleanFilter->ConvertLinesToPointsOff();
  this->OutputNegativeCleanFilter->ConvertPolysToLinesOff();
  this->OutputNegativeCleanFilter->ConvertStripsToPolysOff();

  this->OutputPositiveWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputPositiveWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputPositiveWorldToModelTransformFilter->SetTransform(this->OutputPositiveWorldToModelTransform);

  this->OutputNegativeWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputNegativeWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputNegativeWorldToModelTransformFilter->SetTransform(this->OutputNegativeWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerPlaneCutTool::~vtkSlicerDynamicModelerPlaneCutTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerPlaneCutTool::GetName()
{
  return "Plane cut";
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerPlaneCutTool::CreateEndCap(vtkPlaneCollection* planes, vtkPolyData* originalPolyData, vtkImplicitBoolean* cutFunction, vtkPolyData* outputEndCap)
{
  int operationType = cutFunction->GetOperationType();
  vtkNew<vtkPlaneCollection> intersectionPlanes;
  CreateIntersectionPlanes(planes, operationType, intersectionPlanes);

  vtkNew<vtkClipClosedSurface> capGenerator;
  capGenerator->SetInputData(originalPolyData);
  capGenerator->SetClippingPlanes(intersectionPlanes);
  capGenerator->GenerateClipFaceOutputOn();
  capGenerator->Update();

  if (IntersectionOutputIsPositive(operationType))
    {
    outputEndCap->ShallowCopy(capGenerator->GetClipFaceOutput());
    }
  else
    {
    // Intersection and difference generate the closed negative output. The common cap
    // is reversed here because callers append it to the positive output first.
    vtkNew<vtkReverseSense> reverseSense;
    reverseSense->SetInputData(capGenerator->GetClipFaceOutput());
    reverseSense->ReverseCellsOn();
    reverseSense->ReverseNormalsOn();
    reverseSense->Update();
    outputEndCap->ShallowCopy(reverseSense->GetOutput());
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerPlaneCutTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputPositiveModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(PLANE_CUT_OUTPUT_POSITIVE_MODEL_REFERENCE_ROLE));
  vtkMRMLModelNode* outputNegativeModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(PLANE_CUT_OUTPUT_NEGATIVE_MODEL_REFERENCE_ROLE));
  if (!outputPositiveModelNode && !outputNegativeModelNode)
    {
    // Nothing to output.
    return true;
    }

  vtkNew<vtkImplicitBoolean> planes;
  std::string operationType = this->GetNthInputParameterValue(1, surfaceEditorNode).ToString();
  // Following the convention of VTK, the boolean operation is applied to the negative sides of the inputs to produce the negative side of the output.
  if (operationType == "Intersection")
    {
    planes->SetOperationTypeToIntersection();
    }
  else if (operationType == "Difference")
    {
    planes->SetOperationTypeToDifference();
    }
  else
    {
    planes->SetOperationTypeToUnion();
    }

  std::vector<vtkMRMLNode*> planeNodes;
  surfaceEditorNode->GetNodeReferences(PLANE_CUT_INPUT_PLANE_REFERENCE_ROLE, planeNodes);
  vtkNew<vtkPlaneCollection> planeCollection;
  int planeIndex = 0;
  for (vtkMRMLNode* planeNode : planeNodes)
    {
    vtkMRMLMarkupsPlaneNode* inputPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(planeNode);
    vtkMRMLSliceNode* inputSliceNode = vtkMRMLSliceNode::SafeDownCast(planeNode);
    if (!inputPlaneNode && !inputSliceNode)
      {
      vtkErrorMacro("Invalid input plane nodes!");
      return false;
      }

    double origin_World[3] = { 0.0, 0.0, 0.0 };
    double normal_World[3] = { 0.0, 0.0, 1.0 };
    if (inputPlaneNode)
      {
      inputPlaneNode->GetOriginWorld(origin_World);
      inputPlaneNode->GetNormalWorld(normal_World);
      }
    if (inputSliceNode)
      {
      vtkMatrix4x4* sliceToRAS = inputSliceNode->GetSliceToRAS();
      vtkNew<vtkTransform> sliceToRASTransform;
      sliceToRASTransform->SetMatrix(sliceToRAS);
      sliceToRASTransform->TransformPoint(origin_World, origin_World);
      sliceToRASTransform->TransformVector(normal_World, normal_World);
      }

    vtkNew<vtkPlane> currentPlane;
    currentPlane->SetNormal(normal_World);
    currentPlane->SetOrigin(origin_World);
    planeCollection->AddItem(currentPlane);
    planes->AddFunction(currentPlane);
    ++planeIndex;
    }
  this->PlaneClipper->SetClipFunction(planes);

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(PLANE_CUT_INPUT_MODEL_REFERENCE_ROLE));
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
  if (outputPositiveModelNode && outputPositiveModelNode->GetParentTransformNode())
    {
    outputPositiveModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputPositiveWorldToModelTransform);
    }
  if (outputNegativeModelNode && outputNegativeModelNode->GetParentTransformNode())
    {
    outputNegativeModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputNegativeWorldToModelTransform);
    }

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());
  this->PlaneClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->PlaneClipper->SetGenerateClippedOutput(outputNegativeModelNode != nullptr);

  bool capSurface = this->GetNthInputParameterValue(0, surfaceEditorNode).ToInt() != 0;
  vtkNew<vtkPolyData> cappedPositiveOutput;
  vtkNew<vtkPolyData> cappedNegativeOutput;
  if (capSurface)
    {
    this->InputModelToWorldTransformFilter->Update();
    vtkPolyData* inputModelWorldPolyData = this->InputModelToWorldTransformFilter->GetOutput();
    CreateCappedOutputs(planeCollection, inputModelWorldPolyData, planes->GetOperationType(),
      cappedPositiveOutput, cappedNegativeOutput);
    }

  if (outputPositiveModelNode)
    {
    vtkNew<vtkPolyData> outputMesh;
    if (capSurface)
      {
      outputMesh->ShallowCopy(cappedPositiveOutput);
      }
    else
      {
      this->OutputPositiveCleanFilter->Update();
      outputMesh->ShallowCopy(this->OutputPositiveCleanFilter->GetOutput());
      }

    this->OutputPositiveWorldToModelTransformFilter->SetInputData(outputMesh);
    this->OutputPositiveWorldToModelTransformFilter->Update();
    outputMesh->DeepCopy(this->OutputPositiveWorldToModelTransformFilter->GetOutput());

    MRMLNodeModifyBlocker blocker(outputPositiveModelNode);
    outputPositiveModelNode->SetAndObserveMesh(outputMesh);
    outputPositiveModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  if (outputNegativeModelNode)
    {
    vtkNew<vtkPolyData> outputMesh;
    if (capSurface)
      {
      outputMesh->ShallowCopy(cappedNegativeOutput);
      }
    else
      {
      this->OutputNegativeCleanFilter->Update();
      outputMesh->ShallowCopy(this->OutputNegativeCleanFilter->GetOutput());
      }

    this->OutputNegativeWorldToModelTransformFilter->SetInputData(outputMesh);
    this->OutputNegativeWorldToModelTransformFilter->Update();
    outputMesh->DeepCopy(this->OutputNegativeWorldToModelTransformFilter->GetOutput());

    MRMLNodeModifyBlocker blocker(outputNegativeModelNode);
    outputNegativeModelNode->SetAndObserveMesh(outputMesh);
    outputNegativeModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  return true;
}
