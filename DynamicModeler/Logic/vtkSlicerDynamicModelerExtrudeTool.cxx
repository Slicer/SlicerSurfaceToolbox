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
#include <vtkMRMLMarkupsAngleNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsCurveNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
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
#include <vtkPlane.h>
#include <vtkPlaneSource.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerExtrudeTool);

const char* EXTRUDE_INPUT_MODEL_REFERENCE_ROLE = "Extrude.InputModel";
const char* EXTRUDE_INPUT_MARKUPS_REFERENCE_ROLE = "Extrude.InputMarkups";
const char* EXTRUDE_OUTPUT_MODEL_REFERENCE_ROLE = "Extrude.OutputModel";
const char* EXTRUDE_LENGTH = "Extrude.Length";
const char* EXTRUDE_SCALE = "Extrude.Scale";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerExtrudeTool::vtkSlicerDynamicModelerExtrudeTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointPositionDefinedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointPositionUndefinedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsClosedCurveNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsAngleNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsLineNode");
  NodeInfo inputModel(
    "Model or Markup",
    "Profile to be extruded.",
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
  inputMarkupEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointPositionDefinedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointPositionUndefinedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputMarkupClassNames;
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsLineNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsAngleNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  inputMarkupClassNames->InsertNextValue("vtkMRMLMarkupsClosedCurveNode");
  NodeInfo inputMarkups(
    "Markups",
    "Markups to specify extrusion vector.\n"
    "- Plane or Angle: extrusion vector is the plane normal.\n"
    "- Line: extrusion vector is from the first to the second point of the line.\n"
    "- Point list: extrusion vector is from each model point to the first point of the markup.\n"
    "- Curve or Closed Curve: extrusion vector is best-fitting plane normal.\n"
    "- No markup is selected: extrusion vector is the input model's surface normal.",
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

  ParameterInfo parameterLength(
    "Extrusion length",
    "Absolute length value that is used for computing the extrusion length. It is added to the scaled input vector magnitude: extrusion_length = input_vector_magnitude * scale + length",
    EXTRUDE_LENGTH,
    PARAMETER_DOUBLE,
    5.0);
  this->InputParameterInfo.push_back(parameterLength);

  ParameterInfo parameterScale(
    "Extrusion scale",
    "Input vector magnitude is multiplied by this scale to get the extruction length. Length parameter is added to this scaled vector: extrusion_length = input_vector_magnitude * scale + length",
    EXTRUDE_SCALE,
    PARAMETER_DOUBLE,
    0.0);
  this->InputParameterInfo.push_back(parameterScale);
  this->InputProfileToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputProfileNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputProfileToWorldTransformFilter->SetTransform(this->InputProfileNodeToWorldTransform);

  // Auxiliar plane source is used to create a plane for the input profile when it is a markups plane
  this->AuxiliarPlaneSource = vtkSmartPointer<vtkPlaneSource>::New();
  
  // This is used when the input polydata does not have normals
  this->NormalsFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->NormalsFilter->AutoOrientNormalsOn();

  // This is used when absolute length is used for vector extrusion
  this->AssignAttributeFilter = vtkSmartPointer<vtkAssignAttribute>::New();

  this->ExtrudeFilter = vtkSmartPointer<vtkLinearExtrusionFilter>::New();
  this->ExtrudeFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());

  this->TriangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->TriangleFilter->SetInputConnection(this->ExtrudeFilter->GetOutputPort());

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

  vtkMRMLModelNode* inputProfileModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
  vtkMRMLMarkupsNode* inputProfileMarkupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
  bool profileIsModel = (inputProfileModelNode) && (!inputProfileMarkupsNode);
  bool profileIsMarkup = (!inputProfileModelNode) && (inputProfileMarkupsNode);
  if ((!profileIsModel) && (!profileIsMarkup))
  {
    vtkErrorMacro("Invalid input node!");
    return false;
  }

  if (profileIsModel)
  {
    if (!inputProfileModelNode->GetMesh() || inputProfileModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
      vtkNew<vtkPolyData> outputPolyData;
      outputModelNode->SetAndObservePolyData(outputPolyData);
      return true;
    } 
    else
    {
      if (inputProfileModelNode->GetParentTransformNode())
      {
        inputProfileModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputProfileNodeToWorldTransform);
      }
      this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileModelNode->GetMeshConnection());
    }
  }
  else // profileIsMarkup
  {
    vtkMRMLMarkupsFiducialNode* inputProfileMarkupsPointsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
    vtkMRMLMarkupsLineNode* inputProfileMarkupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
    vtkMRMLMarkupsAngleNode* inputProfileMarkupsAngleNode = vtkMRMLMarkupsAngleNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
    vtkMRMLMarkupsPlaneNode* inputProfileMarkupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
    vtkMRMLMarkupsCurveNode* inputProfileMarkupsCurveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
    vtkMRMLMarkupsClosedCurveNode* inputProfileMarkupsClosedCurveNode = vtkMRMLMarkupsClosedCurveNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MODEL_REFERENCE_ROLE));
    this->InputProfileNodeToWorldTransform->Identity(); // this way the transformFilter is a pass-through
    if (inputProfileMarkupsPointsNode)
    {
      if (!inputProfileMarkupsPointsNode->GetCurveWorld() || inputProfileMarkupsPointsNode->GetCurveWorld()->GetNumberOfPoints() == 0)
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileMarkupsPointsNode->GetCurveWorldConnection());
      }
    }
    else if (inputProfileMarkupsLineNode)
    {
      if (!inputProfileMarkupsLineNode->GetCurveWorld() || inputProfileMarkupsLineNode->GetCurveWorld()->GetNumberOfPoints() == 0)
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileMarkupsLineNode->GetCurveWorldConnection());
      }
    }
    else if (inputProfileMarkupsAngleNode)
    {
      if (!inputProfileMarkupsAngleNode->GetCurveWorld() || inputProfileMarkupsAngleNode->GetCurveWorld()->GetNumberOfPoints() == 0)
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileMarkupsAngleNode->GetCurveWorldConnection());
      }
    }
    else if (inputProfileMarkupsPlaneNode)
    {
      if (!inputProfileMarkupsPlaneNode->GetIsPlaneValid())
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        // Update the plane based on the corner points
        vtkNew<vtkPoints> planeCornerPoints_World;
        inputProfileMarkupsPlaneNode->GetPlaneCornerPointsWorld(planeCornerPoints_World);

        // Update the plane fill
        this->AuxiliarPlaneSource->SetOrigin(planeCornerPoints_World->GetPoint(0));
        this->AuxiliarPlaneSource->SetPoint1(planeCornerPoints_World->GetPoint(1));
        this->AuxiliarPlaneSource->SetPoint2(planeCornerPoints_World->GetPoint(3));
        this->InputProfileToWorldTransformFilter->SetInputConnection(this->AuxiliarPlaneSource->GetOutputPort());
      }
    }
    else if (inputProfileMarkupsCurveNode)
    {
      if (!inputProfileMarkupsCurveNode->GetCurveWorld() || inputProfileMarkupsCurveNode->GetCurveWorld()->GetNumberOfPoints() == 0)
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileMarkupsCurveNode->GetCurveWorldConnection());
      }
    }
    else if (inputProfileMarkupsClosedCurveNode)
    {
      this->InputProfileNodeToWorldTransform->Identity(); // this way the transformFilter is a pass-through
      if (!inputProfileMarkupsClosedCurveNode->GetCurveWorld() || inputProfileMarkupsClosedCurveNode->GetCurveWorld()->GetNumberOfPoints() == 0)
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileMarkupsClosedCurveNode->GetCurveWorldConnection());
      }
    }
  }


  if (outputModelNode && outputModelNode->GetParentTransformNode())
  {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
  }
  else
  {
    this->OutputWorldToModelTransform->Identity();
  }

  this->InputProfileToWorldTransformFilter->Update();
  // with filter input below we'll never create normals unless they don't exist
  this->ExtrudeFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());

  vtkDataArray* normalsArray = nullptr;
  if (this->InputProfileToWorldTransformFilter->GetOutput()
    && this->InputProfileToWorldTransformFilter->GetOutput()->GetPointData())
  {
    normalsArray = this->InputProfileToWorldTransformFilter->GetOutput()->GetPointData()->GetNormals();
  }

  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(EXTRUDE_INPUT_MARKUPS_REFERENCE_ROLE));

  double extrusionLength = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();
  double extrusionScale = this->GetNthInputParameterValue(1, surfaceEditorNode).ToDouble();

  const std::string extrusionArrayName = "__tmp__ExtrusionVectors";

  if (!markupsNode)
  {
    // Normals are used for extrusion
    if (normalsArray)
    {
      // Normals is already available
      this->ExtrudeFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());
    }
    else
    {
      // Create the normals by inserting normals filter before extrusion filter
      this->NormalsFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());
      this->ExtrudeFilter->SetInputConnection(this->NormalsFilter->GetOutputPort());
    }
    this->ExtrudeFilter->SetExtrusionTypeToNormalExtrusion();
    const double magnitude = 1.0; // normal vector magnitude is always 1.0
    this->ExtrudeFilter->SetScaleFactor(magnitude * extrusionScale + extrusionLength);
  }
  else
  {
    // Markups are used for extrusion
    vtkMRMLMarkupsFiducialNode* markupsFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsLineNode* markupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsPlaneNode* markupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsAngleNode* markupsAngleNode = vtkMRMLMarkupsAngleNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsCurveNode* markupsCurveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(markupsNode);
    vtkMRMLMarkupsClosedCurveNode* markupsClosedCurveNode = vtkMRMLMarkupsClosedCurveNode::SafeDownCast(markupsNode);
    bool markupsToUseBestFittingPlane = (markupsAngleNode || markupsCurveNode || markupsClosedCurveNode);
    int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();

    if (markupsToUseBestFittingPlane && (numberOfControlPoints >= 3))
    {
      const double magnitude = 1.0; // normal vector magnitude is always 1.0
      this->ExtrudeFilter->SetScaleFactor(magnitude * extrusionScale + extrusionLength);
      // get control points in world coordinates
      vtkNew<vtkPoints> controlPointsWorld;
      for (int i = 0; i < numberOfControlPoints; ++i)
      {
        double controlPointWorld[3] = { 0, 0, 0 };
        markupsNode->GetNthControlPointPositionWorld(i, controlPointWorld);
        controlPointsWorld->InsertNextPoint(controlPointWorld);
      }
      double bestFitOriginWorld[3] = { 0, 0, 0 };
      double bestFitNormalWorld[3] = { 0, 0, 0 };
      vtkPlane::ComputeBestFittingPlane(controlPointsWorld, bestFitOriginWorld, bestFitNormalWorld);
      this->ExtrudeFilter->SetVector(bestFitNormalWorld);
      this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
    }
    else if (markupsPlaneNode)
    {
      // Plane normal is used for extrusion
      markupsPlaneNode->GetRequiredNumberOfControlPoints();
      const double magnitude = 1.0; // normal vector magnitude is always 1.0
      this->ExtrudeFilter->SetScaleFactor(magnitude * extrusionScale + extrusionLength);
      int planeType = markupsPlaneNode->GetPlaneType();
      if (
        ((numberOfControlPoints == 1) && (planeType == vtkMRMLMarkupsPlaneNode::PlaneTypePointNormal)) ||
        ((numberOfControlPoints == 3) && (planeType == vtkMRMLMarkupsPlaneNode::PlaneType3Points)) ||
        ((numberOfControlPoints >= 3) && (planeType == vtkMRMLMarkupsPlaneNode::PlaneTypePlaneFit))
      )
      {
        double startToEndVector[3] = { 1.0, 0.0, 0.0 };
        markupsPlaneNode->GetNormalWorld(startToEndVector);
        this->ExtrudeFilter->SetVector(startToEndVector);
        this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
      }
    }
    else if ((markupsFiducialNode) && (numberOfControlPoints >= 1))
    {
      // Extrude towards a point

      // No need for normals, so we can always connect to the input model
      this->ExtrudeFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());

      if (extrusionLength == 0)
      {
        // Special case, only scaling
        double center[3] = { 0,0,0 };
        markupsFiducialNode->GetNthControlPointPosition(0, center);
        this->ExtrudeFilter->SetExtrusionPoint(center);
        this->ExtrudeFilter->SetExtrusionTypeToPointExtrusion();
        this->ExtrudeFilter->SetScaleFactor(-extrusionScale);
      }
      else
      {
        // Absolute length is specified, this is not supported directly by the extrusion filter

        this->InputProfileToWorldTransformFilter->Update();
        vtkPolyData* inputPolyData = this->InputProfileToWorldTransformFilter->GetOutput();

        // overwrite normals array with directions of (center-pointAt) array
        vtkNew<vtkFloatArray> extrusionVectorArray;
        extrusionVectorArray->SetName(extrusionArrayName.c_str());
        vtkIdType numberOfPoints = inputPolyData->GetNumberOfPoints();
        vtkPoints* points = inputPolyData->GetPoints();
        extrusionVectorArray->SetNumberOfComponents(3);
        extrusionVectorArray->Allocate(numberOfPoints);
        double center[3] = { 0,0,0 };
        markupsFiducialNode->GetNthControlPointPositionWorld(0, center);
        for (int i = 0; i < numberOfPoints; ++i)
        {
          double surfacePoint[3] = { 0,0,0 };
          points->GetPoint(i, surfacePoint);
          double direction[3] = { 0,0,0 };
          vtkMath::Subtract(center, surfacePoint, direction);
          double magnitude = vtkMath::Normalize(direction);
          vtkMath::MultiplyScalar(direction, magnitude * extrusionScale + extrusionLength);
          extrusionVectorArray->InsertNextTuple(direction);
        }

        inputPolyData->GetPointData()->AddArray(extrusionVectorArray);
        this->AssignAttributeFilter->SetInputData(inputPolyData);
        this->AssignAttributeFilter->Assign(extrusionArrayName.c_str(), vtkDataSetAttributes::NORMALS, vtkAssignAttribute::POINT_DATA);
        this->ExtrudeFilter->SetInputConnection(this->AssignAttributeFilter->GetOutputPort());
        this->ExtrudeFilter->SetExtrusionTypeToNormalExtrusion();
        this->ExtrudeFilter->SetScaleFactor(1.0);
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
      double magnitude = vtkMath::Normalize(startToEndVector);
      this->ExtrudeFilter->SetVector(startToEndVector);
      this->ExtrudeFilter->SetExtrusionTypeToVectorExtrusion();
      this->ExtrudeFilter->SetScaleFactor(magnitude * extrusionScale + extrusionLength);
    }
  }

  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  // Remove temporary array
  outputMesh->GetPointData()->RemoveArray(extrusionArrayName.c_str());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}
