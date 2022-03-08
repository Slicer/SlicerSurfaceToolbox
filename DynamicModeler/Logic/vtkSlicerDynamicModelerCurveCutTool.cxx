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

#include "vtkSlicerDynamicModelerCurveCutTool.h"

// DynamicModeler MRML includes
#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkCleanPolyData.h>
#include <vtkClipPolyData.h>
#include <vtkCommand.h>
#include <vtkConnectivityFilter.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkSelectPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>

// vtkAddon
#include <vtkAddonMathUtilities.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerCurveCutTool);

namespace
{
  const char* CURVE_CUT_INPUT_MODEL_REFERENCE_ROLE = "CurveCut.InputModel";
  const char* CURVE_CUT_INPUT_CURVE_REFERENCE_ROLE = "CurveCut.InputCurve";
  const char* CURVE_CUT_INPUT_INSIDE_POINT_REFERENCE_ROLE = "CurveCut.InsidePoint";
  const char* CURVE_CUT_STRAIGHT_CUT = "CurveCut.StraightCut";
  const char* CURVE_CUT_OUTPUT_INSIDE_MODEL_REFERENCE_ROLE = "CurveCut.OutputInside";
  const char* CURVE_CUT_OUTPUT_OUTSIDE_MODEL_REFERENCE_ROLE = "CurveCut.OutputOutside";
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerCurveCutTool::vtkSlicerDynamicModelerCurveCutTool()
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
    "Model",
    "Model node to be cut with the curve.",
    inputModelClassNames,
    CURVE_CUT_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputCurveEvents;
  inputCurveEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputCurveEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputCurveEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputCurveClassNames;
  inputCurveClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  NodeInfo inputCurve(
    "Curve",
    "Curve node to cut the model node.",
    inputCurveClassNames,
    CURVE_CUT_INPUT_CURVE_REFERENCE_ROLE,
    true,
    false,
    inputCurveEvents
    );
  this->InputNodeInfo.push_back(inputCurve);

  vtkNew<vtkStringArray> insidePointListClassNames;
  insidePointListClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  NodeInfo insidePoint(
    "Inside point",
    "Closest region to the first point of this point list will be used as 'inside'. If not specified then the smallest region is used as 'inside'.",
    insidePointListClassNames,
    CURVE_CUT_INPUT_INSIDE_POINT_REFERENCE_ROLE,
    false,
    false,
    inputCurveEvents
  );
  this->InputNodeInfo.push_back(insidePoint);

  ParameterInfo parameterStraightCut(
    "Straight cut",
    "If enabled then the surface will be cut as close as possible to the curve, otherwise edges of the original mesh are preserved",
    CURVE_CUT_STRAIGHT_CUT,
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterStraightCut); 

  /////////
  // Outputs
  NodeInfo outputInsideModel(
    "Inside model",
    "Output model containing region inside the cut.",
    inputModelClassNames,
    CURVE_CUT_OUTPUT_INSIDE_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputInsideModel);

  NodeInfo outputOutsideModel(
    "Outside model",
    "Output model containing region outside the cut.",
    inputModelClassNames,
    CURVE_CUT_OUTPUT_OUTSIDE_MODEL_REFERENCE_ROLE,
    false,
    false
  );
  this->OutputNodeInfo.push_back(outputOutsideModel);

  this->CleanFilter = vtkSmartPointer<vtkCleanPolyData>::New();

  this->InputModelToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelToWorldTransformFilter->SetInputConnection(this->CleanFilter->GetOutputPort());
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelToWorldTransform);

  this->SelectionFilter = vtkSmartPointer<vtkSelectPolyData>::New();
  this->SelectionFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->SelectionFilter->SetEdgeSearchModeToDijkstra();

  // Further connections depend on cut mode (straight or whole cells)
  this->ClipFilter = vtkSmartPointer<vtkClipPolyData>::New();
  this->ClipFilter->InsideOutOn();

  this->ConnectivityFilter = vtkSmartPointer<vtkConnectivityFilter>::New();
  this->ConnectivityFilter->SetInputConnection(this->ClipFilter->GetOutputPort());

  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransformFilter->SetTransform(this->OutputWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerCurveCutTool::~vtkSlicerDynamicModelerCurveCutTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerCurveCutTool::GetName()
{
  return "Curve cut";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerCurveCutTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputInsideModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(CURVE_CUT_OUTPUT_INSIDE_MODEL_REFERENCE_ROLE));
  vtkMRMLModelNode* outputOutsideModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(CURVE_CUT_OUTPUT_OUTSIDE_MODEL_REFERENCE_ROLE));
  if (!outputInsideModelNode && !outputOutsideModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(CURVE_CUT_INPUT_MODEL_REFERENCE_ROLE));
  if (!inputModelNode || !inputModelNode->GetPolyData())
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  vtkMRMLNode* inputNode = surfaceEditorNode->GetNodeReference(CURVE_CUT_INPUT_CURVE_REFERENCE_ROLE);
  vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
  if (!curveNode)
    {
    vtkErrorMacro("Invalid input curve node!");
    return false;
    }

  double bounds_World[6] = {0.0};
  inputModelNode->GetRASBounds(bounds_World);

  this->CleanFilter->SetInputData(inputModelNode->GetPolyData());
  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelToWorldTransform);
    }
  else
    {
    this->InputModelToWorldTransform->Identity();
    }

  this->SelectionFilter->SetLoop(curveNode->GetCurvePointsWorld());

  vtkMRMLMarkupsFiducialNode* insidePointNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(
    surfaceEditorNode->GetNthNodeReference(CURVE_CUT_INPUT_INSIDE_POINT_REFERENCE_ROLE, 0));
  if (insidePointNode && insidePointNode->GetNumberOfControlPoints() > 0)
    {
    double insidePointPositionWorld[3] = { 0.0, 0.0, 0.0 };
    insidePointNode->GetNthControlPointPositionWorld(0, insidePointPositionWorld);
    this->SelectionFilter->SetSelectionModeToClosestPointRegion();
    this->SelectionFilter->SetClosestPoint(insidePointPositionWorld);
    }
  else
    {
    this->SelectionFilter->SetSelectionModeToSmallestRegion();
    }

  bool straightCut = vtkVariant(surfaceEditorNode->GetAttribute(CURVE_CUT_STRAIGHT_CUT)).ToInt() != 0;
  this->SelectionFilter->SetGenerateSelectionScalars(straightCut);

  // Straight cut:
  //
  //   InputModelToWorldTransformFilter -> SelectionFilter
  //   -> ClipFilter -> ConnectivityFilter -> OutputWorldToModelTransform
  //
  // Whole cell cut:
  //
  //   InputModelToWorldTransformFilter -> SelectionFilter
  //   -> OutputWorldToModelTransform

  vtkSmartPointer<vtkPolyData> outputInsideMesh;
  vtkSmartPointer<vtkPolyData> outputOutsideMesh;

  if (outputInsideModelNode)
    {
    if (outputInsideModelNode->GetParentTransformNode())
      {
      outputInsideModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
      }
    else
      {
      this->OutputWorldToModelTransform->Identity();
      }
    }
  if (outputOutsideModelNode)
    {
    if (outputOutsideModelNode->GetParentTransformNode())
      {
      outputOutsideModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
      }
    else
      {
      this->OutputWorldToModelTransform->Identity();
      }
    }

  if (straightCut)
    {
    // straight cut
    this->ClipFilter->SetInputConnection(this->SelectionFilter->GetOutputPort());
    this->OutputWorldToModelTransformFilter->SetInputConnection(this->ConnectivityFilter->GetOutputPort());
    this->ClipFilter->SetGenerateClippedOutput(outputOutsideModelNode != nullptr);
    if (outputInsideModelNode)
      {
      this->ConnectivityFilter->SetInputConnection(this->ClipFilter->GetOutputPort());
      this->OutputWorldToModelTransformFilter->Update();
      outputInsideMesh = vtkSmartPointer<vtkPolyData>::New();
      outputInsideMesh->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());
      }
    if (outputOutsideModelNode)
      {
      this->ConnectivityFilter->SetInputConnection(this->ClipFilter->GetClippedOutputPort());
      this->OutputWorldToModelTransformFilter->Update();
      outputOutsideMesh = vtkSmartPointer<vtkPolyData>::New();
      outputOutsideMesh->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());
      }
    }
  else
    {
    // whole cells
    this->SelectionFilter->SetGenerateUnselectedOutput(outputOutsideModelNode != nullptr);
    if (outputInsideModelNode)
      {
      this->OutputWorldToModelTransformFilter->SetInputConnection(this->SelectionFilter->GetOutputPort());
      this->OutputWorldToModelTransformFilter->Update();
      outputInsideMesh = vtkSmartPointer<vtkPolyData>::New();
      outputInsideMesh->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());
      }
    if (outputOutsideModelNode)
      {
      this->OutputWorldToModelTransformFilter->SetInputConnection(this->SelectionFilter->GetUnselectedOutputPort());
      this->OutputWorldToModelTransformFilter->Update();
      outputOutsideMesh = vtkSmartPointer<vtkPolyData>::New();
      outputOutsideMesh->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());
      }
    }

  // Write polydata to output nodes
  if (outputInsideModelNode)
    {
    MRMLNodeModifyBlocker blocker(outputInsideModelNode);
    outputInsideModelNode->SetAndObserveMesh(outputInsideMesh);
    outputInsideModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }
  if (outputOutsideModelNode)
    {
    MRMLNodeModifyBlocker blocker(outputOutsideModelNode);
    outputOutsideModelNode->SetAndObserveMesh(outputOutsideMesh);
    outputOutsideModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }
  return true;
}