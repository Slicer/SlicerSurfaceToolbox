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

#include "vtkSlicerDynamicModelerExtrudeTool.h"

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
#include <vtkLinearExtrusionFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerExtrudeTool);

const char* EXTRUDE_INPUT_MODEL_REFERENCE_ROLE = "Extrude.InputModel";
const char* EXTRUDE_INPUT_MARKUPS_REFERENCE_ROLE = "Extrude.InputMarkups";
const char* EXTRUDE_OUTPUT_MODEL_REFERENCE_ROLE = "Extrude.OutputModel";
const char* EXTRUDE_ALONG_NORMAL = "Extrude.AlongNormal";
const char* EXTRUDE_LENGTH_MODE = "Extrude.LengthMode";
const char* EXTRUDE_VALUE = "Extrude.ExtrusionValue";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerExtrudeTool::vtkSlicerDynamicModelerExtrudeTool()
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
    "Model to be extruded.",
    inputModelClassNames,
    EXTRUDE_INPUT_MODEL_REFERENCE_ROLE,
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
    "Markups to specify extrusion direction. Not used if mode is set to surface normal.",
    inputMarkupClassNames,
    EXTRUDE_INPUT_MARKUPS_REFERENCE_ROLE,
    /*required*/ false,
    /*repeatable*/ false,
    inputMarkupEvents
  );
  this->InputNodeInfo.push_back(inputMarkups);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Extruded model",
    "Result of the extrusion operation.",
    inputModelClassNames,
    EXTRUDE_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
  );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters

  ParameterInfo parameterExtrudeAlongNormal(
    "Extrude along normals",
    "Each mesh cell will be extruded along its normal with a fixed length. Markups will be ignored. Uncheck to use markups or scaled length mode.",
    EXTRUDE_ALONG_NORMAL,
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterExtrudeAlongNormal);

  ParameterInfo parameterExtrusionValue(
    "Extrusion value",
    "Value used for fixed length or multiplied by point position. Use negative value for reversing extrusion direction.",
    EXTRUDE_VALUE,
    PARAMETER_DOUBLE,
    1.0);
  this->InputParameterInfo.push_back(parameterExtrusionValue);

  ParameterInfo parameterLengthMode(
    "Length mode",
    "Can be fixed length or scaled length.",
    EXTRUDE_LENGTH_MODE,
    PARAMETER_STRING_ENUM,
    "Fixed");

  vtkNew<vtkStringArray> possibleValues;
  parameterLengthMode.PossibleValues = possibleValues;
  parameterLengthMode.PossibleValues->InsertNextValue("Fixed");
  parameterLengthMode.PossibleValues->InsertNextValue("Scaled");
  this->InputParameterInfo.push_back(parameterLengthMode);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->NormalsFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->NormalsFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->NormalsFilter->AutoOrientNormalsOn();
  
  this->ExtrudeFilter = vtkSmartPointer<vtkLinearExtrusionFilter>::New();
  this->ExtrudeFilter->SetInputConnection(this->NormalsFilter->GetOutputPort());

  this->TriangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->TriangleFilter->SetInputConnection(this->ExtrudeFilter->GetOutputPort());

  //this->NormalsFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
  //this->NormalsFilter->SetInputConnection(this->TriangleFilter->GetOutputPort());
  //this->NormalsFilter->AutoOrientNormalsOn();

  this->OutputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputModelToWorldTransformFilter->SetTransform(this->OutputWorldToModelTransform);
  this->OutputModelToWorldTransformFilter->SetInputConnection(this->TriangleFilter->GetOutputPort());
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerExtrudeTool::~vtkSlicerDynamicModelerExtrudeTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerExtrudeTool::GetName()
{
  return "Extrude";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerExtrudeTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
  {
    vtkErrorMacro("Invalid number of inputs");
    return false;
  }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
  {
    // Nothing to output
    return true;
  }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
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
  this->NormalsFilter->Update();

  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MARKUPS_REFERENCE_ROLE));

  vtkMRMLMarkupsFiducialNode* markupsFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsLineNode* markupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsPlaneNode* markupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);

  double extrusionValue = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();
  bool extrudeAlongNormal = vtkVariant(surfaceEditorNode->GetAttribute(EXTRUDE_ALONG_NORMAL)).ToInt() != 0;
  if (extrudeAlongNormal)
  {
    this->ExtrudeFilter->SetExtrusionTypeToNormalExtrusion();
    this->ExtrudeFilter->SetScaleFactor(extrusionValue);
  }
  else
  {
    std::string selectionAlgorithm = this->GetNthInputParameterValue(2, surfaceEditorNode).ToString();
    if (selectionAlgorithm == "Fixed")
    {
      this->ExtrudeFilter->SetScaleFactor(extrusionValue);
      if (markupsFiducialNode)
      {
        double center[3] = { 0,0,0 };
        int controlPointIndex = markupsFiducialNode->GetNthControlPointIndexByPositionStatus(0, vtkMRMLMarkupsNode::PositionDefined);
        if (markupsFiducialNode->GetNumberOfControlPoints() >= 1)
        {
          markupsFiducialNode->GetNthControlPointPosition(0, center);
          //this->ExtrudeFilter->SetExtrusionPoint(center);
          //this->ExtrudeFilter->SetExtrusionTypeToPointExtrusion();
          ////this->ExtrudeFilter->SetScaleFactor(extrusionValue); // negative?
          //this->ExtrudeFilter->SetScaleFactor(-1);
          vtkFloatArray* normalsArray = dynamic_cast<vtkFloatArray*>(
            this->NormalsFilter->GetOutput()->GetPointData()->GetArray("Normals"));
          for (int i = 0; i < normalsArray->GetNumberOfTuples(); ++i)
            {
              double pointAt[3] = { 0,0,0 };
              this->NormalsFilter->GetOutput()->GetPoint(i, pointAt);
              double direction[3] = { 0,0,0 };
              vtkMath::Subtract(center, pointAt, direction);
              vtkMath::Normalize(direction);
              normalsArray->SetTuple3(i,direction[0],direction[1],direction[2]);
            }
          this->ExtrudeFilter->SetExtrusionTypeToNormalExtrusion();
        }
      }
      else if (markupsLineNode)
      {
        double startToEndVector[3] = { 1.0, 0.0, 0.0 };
        double startPos[3] = { 0,0,0 };
        double endPos[3] = { 1,0,0 };
        markupsLineNode->GetLineStartPositionWorld(startPos);
        markupsLineNode->GetLineEndPositionWorld(endPos);
        // startToEndVector = endPos - startPos
        vtkMath::Subtract(endPos, startPos, startToEndVector);
        vtkMath::Normalize(startToEndVector);
        this->ExtrudeFilter->SetVector(startToEndVector);
        this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
      }
      else if (markupsPlaneNode)
      {
        double startToEndVector[3] = { 1.0, 0.0, 0.0 };
        markupsPlaneNode->GetNormalWorld(startToEndVector);
        this->ExtrudeFilter->SetVector(startToEndVector);
        this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
      }
    }
    else if (selectionAlgorithm == "Scaled")
    {
      double placeHolder[3] = { 1,0,0 };
    }
  }

  //this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
  //this->ExtrudeFilter->SetVector(0, 0, extrusionValue);

  //this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
  //this->ExtrudeFilter->SetVector(0, 0, 1);
  //this->ExtrudeFilter->SetScaleFactor(extrusionValue);
  
  //this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();

  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}
