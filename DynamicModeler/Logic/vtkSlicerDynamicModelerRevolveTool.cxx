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

#include "vtkSlicerDynamicModelerRevolveTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkRotationalExtrusionFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>
#include <vtkFeatureEdges.h>
#include <vtkAppendPolyData.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerRevolveTool);

const char* REVOLVE_INPUT_MODEL_REFERENCE_ROLE = "Revolve.InputModel";
const char* REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE = "Revolve.InputMarkups";
const char* REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE = "Revolve.OutputModel";
const char* REVOLVE_ANGLE_DEGREES = "Revolve.AngleDegrees";
const char* REVOLVE_AXIS_AT_ORIGIN = "Revolve.AxisAtOrigin";
const char* REVOLVE_TRANSLATE_ALONG_AXIS_DISTANCE = "Revolve.TranslateDistanceAlongAxis";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRevolveTool::vtkSlicerDynamicModelerRevolveTool()
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
    "Model to be revolved.",
    inputModelClassNames,
    REVOLVE_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputMarkupEvents;
  inputMarkupEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputMarkupClassNames;
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsLineNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  NodeInfo inputMarkups(
    "Markups",
    "Markups to specify spatial revolution axis. Normal for plane, superior axis for a point.",
    inputMarkupClassNames,
    REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE,
    /*required*/ true,
    /*repeatable*/ false,
    inputMarkupEvents
  );
  this->InputNodeInfo.push_back(inputMarkups);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Revolved model",
    "Result of the revolving operation.",
    inputModelClassNames,
    REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
  );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters

  ParameterInfo parameterRotationAngleDegress(
    "Rotation degrees",
    "Rotation angle in degrees.",
    REVOLVE_ANGLE_DEGREES,
    PARAMETER_DOUBLE,
    90.0);
  this->InputParameterInfo.push_back(parameterRotationAngleDegress);

  ParameterInfo parameterRotationAxisAtOrigin(
    "Rotation axis at origin",
    "If true, the revolution will be done around the origin.",
    REVOLVE_AXIS_AT_ORIGIN,
    PARAMETER_BOOL,
    false);
  this->InputParameterInfo.push_back(parameterRotationAxisAtOrigin);

  ParameterInfo parameterTranslationAlongAxisDistance(
    "Translate along axis",
    "Translation distance during the swept.",
    REVOLVE_TRANSLATE_ALONG_AXIS_DISTANCE,
    PARAMETER_DOUBLE,
    0.0);
  this->InputParameterInfo.push_back(parameterTranslationAlongAxisDistance);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->ModelingTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->ModelingTransform = vtkSmartPointer<vtkTransform>::New();
  this->ModelingTransformFilter->SetTransform(this->ModelingTransform);

  this->CapTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->CapTransform = vtkSmartPointer<vtkTransform>::New();
  this->CapTransformFilter->SetTransform(this->CapTransform);

  this->BoundaryEdgesFilter = vtkSmartPointer<vtkFeatureEdges>::New();
  //this->BoundaryEdgesFilter->SetInputConnection();
  this->BoundaryEdgesFilter->BoundaryEdgesOn();
  this->BoundaryEdgesFilter->FeatureEdgesOff();
  this->BoundaryEdgesFilter->NonManifoldEdgesOff();
  this->BoundaryEdgesFilter->ManifoldEdgesOff();
  this->BoundaryEdgesFilter->PassLinesOn();

  this->RevolveFilter = vtkSmartPointer<vtkRotationalExtrusionFilter>::New();
  this->RevolveFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  this->AppendFilter = vtkSmartPointer<vtkAppendPolyData>::New();
  //this->AppendFilter->AddInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->AppendFilter->AddInputConnection(this->RevolveFilter->GetOutputPort());
  //this->AppendFilter->AddInputConnection(this->CapTransformFilter->GetOutputPort());

  this->ResamplingTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->ResamplingTransform = vtkSmartPointer<vtkTransform>::New();
  this->ResamplingTransformFilter->SetTransform(this->ResamplingTransform);

  this->TriangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->TriangleFilter->SetInputConnection(this->RevolveFilter->GetOutputPort());

  this->NormalsFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
  //this->NormalsFilter->AutoOrientNormalsOn();
  //this->NormalsFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  this->OutputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputModelToWorldTransformFilter->SetTransform(this->OutputWorldToModelTransform);
  this->OutputModelToWorldTransformFilter->SetInputConnection(this->TriangleFilter->GetOutputPort());
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRevolveTool::~vtkSlicerDynamicModelerRevolveTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerRevolveTool::GetName()
{
  return "Revolve";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRevolveTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
  {
    vtkErrorMacro("Invalid number of inputs");
    return false;
  }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
  {
    // Nothing to output
    return true;
  }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_MODEL_REFERENCE_ROLE));
  if (!inputModelNode)
  {
    vtkErrorMacro("Invalid input model node!");
    return false;
  }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
  {
    vtkNew<vtkPolyData> outputPolyData;
    outputModelNode->SetAndObservePolyData(outputPolyData);
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
  if (outputModelNode && outputModelNode->GetParentTransformNode())
  {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
  }
  else
  {
    this->OutputWorldToModelTransform->Identity();
  }

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());

  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE));

  if (markupsNode == nullptr)
  {
    return true;
  }

  int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();
  if (numberOfControlPoints == 0)
  {
    return true;
  }

  vtkMRMLMarkupsFiducialNode* markupsFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsLineNode* markupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsPlaneNode* markupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);
  
  if ((markupsLineNode) && (numberOfControlPoints != 2))
  {
    return true;
  }

  double rotationAngleDegress = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();
  bool axisAtOrigin = vtkVariant(surfaceEditorNode->GetAttribute(REVOLVE_AXIS_AT_ORIGIN)).ToInt() != 0;
  double translationAlongAxisDistance = this->GetNthInputParameterValue(2, surfaceEditorNode).ToDouble();

  if (markupsFiducialNode)
  {
    double placeholder = 0.;
  }
  if (markupsLineNode)
  {
    double placeholder = 0.;
  }
  if (markupsPlaneNode)
  {
    double placeholder = 0.;
  }

  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}