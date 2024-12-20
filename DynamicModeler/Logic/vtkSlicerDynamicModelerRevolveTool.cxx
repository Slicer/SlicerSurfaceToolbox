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
#include <vtkLinearExtrusionFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerRevolveTool);

const char* REVOLVE_INPUT_MODEL_REFERENCE_ROLE = "Revolve.InputModel";
const char* REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE = "Revolve.InputMarkups";
const char* REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE = "Revolve.OutputModel";
const char* REVOLVE_LENGTH_MODE_ABSOLUTE = "Revolve.ExtrusionLengthModeAbsolute";
const char* REVOLVE_VALUE = "Revolve.ExtrusionLength";

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
    "Markups to specify extrusion direction. Not used if mode is set to surface normal.",
    inputMarkupClassNames,
    REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE,
    /*required*/ false,
    /*repeatable*/ false,
    inputMarkupEvents
  );
  this->InputNodeInfo.push_back(inputMarkups);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Revolved model",
    "Result of the extrusion operation.",
    inputModelClassNames,
    REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
  );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters

  ParameterInfo parameterLengthMode(
    "Extrusion length mode absolute",
    "If absolute then the extrusion length is set to 'Extrusion length' parameter value. If relative then the length defined by the input markup will be multiplied by the 'Extrusion length' parameter value to compute the extrusion length.",
    REVOLVE_LENGTH_MODE_ABSOLUTE,
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterLengthMode);

  ParameterInfo parameterExtrusionValue(
    "Extrusion length",
    "Absolute length value or relative scaling value (depending on 'Extrusion length mode' parameter) that is used for computing the extrusion length.",
    REVOLVE_VALUE,
    PARAMETER_DOUBLE,
    1.0);
  this->InputParameterInfo.push_back(parameterExtrusionValue);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->NormalsFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->NormalsFilter->AutoOrientNormalsOn();
  //this->NormalsFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  
  this->RevolveFilter = vtkSmartPointer<vtkLinearExtrusionFilter>::New();
  //this->RevolveFilter->SetInputConnection(this->NormalsFilter->GetOutputPort());
  this->RevolveFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  this->TriangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->TriangleFilter->SetInputConnection(this->RevolveFilter->GetOutputPort());

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
  this->InputModelToWorldTransformFilter->Update();
  // with filter input below we'll never create normals unless they don't exist
  this->RevolveFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  // backup current normals
  vtkFloatArray* normalsArray = dynamic_cast<vtkFloatArray*>(
    this->InputModelToWorldTransformFilter->GetOutput()->GetPointData()->GetArray("Normals"));
  vtkFloatArray* normalsArrayBackup;
  if (normalsArray)
  {
    normalsArrayBackup->DeepCopy(normalsArray);
  }

  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE));

  double extrusionValue = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();

  if (markupsNode == nullptr)
  {
    if (normalsArray == nullptr) // then create the normals
    {
      this->NormalsFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
      this->RevolveFilter->SetInputConnection(this->NormalsFilter->GetOutputPort());
    }
    this->RevolveFilter->SetExtrusionTypeToNormalExtrusion();
    this->RevolveFilter->SetScaleFactor(extrusionValue);
  }
  else
  {
    bool lengthModeAbsolute = vtkVariant(surfaceEditorNode->GetAttribute(REVOLVE_LENGTH_MODE_ABSOLUTE)).ToInt() != 0;

    vtkMRMLMarkupsFiducialNode* markupsFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsLineNode* markupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsPlaneNode* markupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);
    int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();
    
    if (markupsPlaneNode)
    {
      this->RevolveFilter->SetScaleFactor(extrusionValue);
      int planeType = markupsPlaneNode->GetPlaneType();
      if (
        ((numberOfControlPoints == 1) && (planeType == vtkMRMLMarkupsPlaneNode::PlaneTypePointNormal)) or
        ((numberOfControlPoints == 3) && (planeType == vtkMRMLMarkupsPlaneNode::PlaneType3Points)) or
        ((numberOfControlPoints >= 3) && (planeType == vtkMRMLMarkupsPlaneNode::PlaneTypePlaneFit))
      )
      {
        double startToEndVector[3] = { 1.0, 0.0, 0.0 };
        markupsPlaneNode->GetNormalWorld(startToEndVector);
        this->RevolveFilter->SetVector(startToEndVector);
        this->RevolveFilter->SetExtrusionTypeToVectorExtrusion();
      }
    }
    
    if (lengthModeAbsolute)
    {
      if ((markupsFiducialNode) && (numberOfControlPoints >= 1))
      {
        vtkFloatArray* auxNormalsArray;
        if (normalsArray == nullptr)
        {
          this->NormalsFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
          this->NormalsFilter->Update();
          auxNormalsArray = dynamic_cast<vtkFloatArray*>(
            this->NormalsFilter->GetOutput()->GetPointData()->GetArray("Normals"));
        }
        // overwrite normals array with directions of (center-pointAt) array
        double center[3] = { 0,0,0 };
        markupsFiducialNode->GetNthControlPointPosition(0, center);
        for (int i = 0; i < auxNormalsArray->GetNumberOfTuples(); ++i)
          {
            double pointAt[3] = { 0,0,0 };
            this->NormalsFilter->GetOutput()->GetPoint(i, pointAt);
            double direction[3] = { 0,0,0 };
            vtkMath::Subtract(center, pointAt, direction);
            vtkMath::Normalize(direction);
            auxNormalsArray->SetTuple3(i,direction[0],direction[1],direction[2]);
          }
        this->RevolveFilter->SetInputConnection(this->NormalsFilter->GetOutputPort());
        this->RevolveFilter->SetExtrusionTypeToNormalExtrusion();
        this->RevolveFilter->SetScaleFactor(extrusionValue);
        this->RevolveFilter->Update();
        if (normalsArray)
        {
          this->RevolveFilter->GetOutput()->GetPointData()->SetNormals(normalsArrayBackup);
        }
      }
      else if ((markupsLineNode) && (numberOfControlPoints == 2))
      {
        double startToEndVector[3] = { 1.0, 0.0, 0.0 };
        double startPos[3] = { 0,0,0 };
        double endPos[3] = { 1,0,0 };
        markupsLineNode->GetLineStartPositionWorld(startPos);
        markupsLineNode->GetLineEndPositionWorld(endPos);
        vtkMath::Subtract(endPos, startPos, startToEndVector);
        vtkMath::Normalize(startToEndVector);
        this->RevolveFilter->SetVector(startToEndVector);
        this->RevolveFilter->SetExtrusionTypeToVectorExtrusion();
        this->RevolveFilter->SetScaleFactor(extrusionValue);
      }
    }
    else // lengthMode is relative
    {
      if ((markupsFiducialNode) && (markupsFiducialNode->GetNumberOfControlPoints() >= 1))
      {
        double center[3] = { 0,0,0 };
        markupsFiducialNode->GetNthControlPointPosition(0, center);
        this->RevolveFilter->SetExtrusionPoint(center);
        this->RevolveFilter->SetExtrusionTypeToPointExtrusion();
        this->RevolveFilter->SetScaleFactor(-extrusionValue);
      }
      else if ((markupsLineNode) && (numberOfControlPoints == 2))
      {
        double startToEndVector[3] = { 1.0, 0.0, 0.0 };
        double startPos[3] = { 0,0,0 };
        double endPos[3] = { 1,0,0 };
        markupsLineNode->GetLineStartPositionWorld(startPos);
        markupsLineNode->GetLineEndPositionWorld(endPos);
        vtkMath::Subtract(endPos, startPos, startToEndVector);
        this->RevolveFilter->SetVector(startToEndVector);
        this->RevolveFilter->SetExtrusionTypeToVectorExtrusion();
        this->RevolveFilter->SetScaleFactor(extrusionValue);
      }
    }
  }

  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}