/*==============================================================================

  This dynamic modeler tool was developed by Mauro I. Dominguez, Independent
  as Ad-Honorem work.

  Copyright (c) All Rights Reserved.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#include "vtkSlicerDynamicModelerRevolveTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsAngleNode.h>
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
#include <vtkRotationalExtrusionFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkFeatureEdges.h>
#include <vtkAppendPolyData.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkPlane.h>
#include <vtkPlaneSource.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerRevolveTool);

const char* REVOLVE_INPUT_PROFILE_REFERENCE_ROLE = "Revolve.InputProfile";
const char* REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE = "Revolve.InputMarkups";
const char* REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE = "Revolve.OutputModel";
const char* REVOLVE_ANGLE_DEGREES = "Revolve.AngleDegrees";
const char* REVOLVE_AXIS_IS_AT_ORIGIN = "Revolve.AxisIsAtOrigin";
const char* REVOLVE_TRANSLATE_DISTANCE_ALONG_AXIS = "Revolve.TranslateDistanceAlongAxis";
const char* REVOLVE_DELTA_RADIUS = "Revolve.DeltRadius"; 

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRevolveTool::vtkSlicerDynamicModelerRevolveTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputProfileEvents;
  inputProfileEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputProfileEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputProfileEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputProfileEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointPositionDefinedEvent);
  inputProfileEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointPositionUndefinedEvent);
  inputProfileEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsLineNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsAngleNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  inputModelClassNames->InsertNextValue("vtkMRMLMarkupsClosedCurveNode");
  NodeInfo inputProfile(
    "Model or Markup",
    "Profile to be revolved.",
    inputModelClassNames,
    REVOLVE_INPUT_PROFILE_REFERENCE_ROLE,
    true,
    false,
    inputProfileEvents
  );
  this->InputNodeInfo.push_back(inputProfile);

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
    "Revolution axis",
    "Markups to specify spatial revolution axis. Normal for plane and angle. Superior axis for a point. Best fitting plane normal for curve and closed curve. The direction of rotation is determined from the direction of the rotation axis by the right hand rule.",
    inputMarkupClassNames,
    REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE,
    /*required*/ true,
    /*repeatable*/ false,
    inputMarkupEvents
  );
  this->InputNodeInfo.push_back(inputMarkups);

  /////////
  // Outputs
  vtkNew<vtkStringArray> outputModelClassNames;
  outputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo outputModel(
    "Revolved model",
    "Result of the revolving operation.",
    outputModelClassNames,
    REVOLVE_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
  );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters

  ParameterInfo parameterRotationAngleDegrees(
    "Rotation degrees",
    "Rotation angle in degrees. Ignored for angle markup.",
    REVOLVE_ANGLE_DEGREES,
    PARAMETER_DOUBLE,
    90.0,
    2,
    1);

  vtkNew<vtkDoubleArray> anglesRange;
  parameterRotationAngleDegrees.NumbersRange = anglesRange;
  parameterRotationAngleDegrees.NumbersRange->SetNumberOfComponents(1);
  parameterRotationAngleDegrees.NumbersRange->SetNumberOfValues(2);
  parameterRotationAngleDegrees.NumbersRange->SetValue(0, -3600);
  parameterRotationAngleDegrees.NumbersRange->SetValue(1, 3600);

  this->InputParameterInfo.push_back(parameterRotationAngleDegrees);


  ParameterInfo parameterRotationAxisIsAtOrigin(
    "Rotation axis is at origin",
    "If true, the revolution will be done around the origin of the world coordinate system.",
    REVOLVE_AXIS_IS_AT_ORIGIN,
    PARAMETER_BOOL,
    false);
  this->InputParameterInfo.push_back(parameterRotationAxisIsAtOrigin);


  ParameterInfo parameterTranslationDistanceAlongAxis(
    "Translate along axis",
    "Amount of translation along the rotation axis during the entire rotational sweep.",
    REVOLVE_TRANSLATE_DISTANCE_ALONG_AXIS,
    PARAMETER_DOUBLE,
    0.0,
    2,
    10);

  vtkNew<vtkDoubleArray> translationAlongAxisRange;
  parameterTranslationDistanceAlongAxis.NumbersRange = translationAlongAxisRange;
  parameterTranslationDistanceAlongAxis.NumbersRange->SetNumberOfComponents(1);
  parameterTranslationDistanceAlongAxis.NumbersRange->SetNumberOfValues(2);
  parameterTranslationDistanceAlongAxis.NumbersRange->SetValue(0, -1000);
  parameterTranslationDistanceAlongAxis.NumbersRange->SetValue(1, 1000);

  this->InputParameterInfo.push_back(parameterTranslationDistanceAlongAxis);


  ParameterInfo parameterDeltaRadius(
    "Change in radius during revolve process",
    "Difference factor between the rotation start and end radius after the rotational sweep.",
    REVOLVE_DELTA_RADIUS,
    PARAMETER_DOUBLE,
    0.0,
    2,
    0.1);

  vtkNew<vtkDoubleArray> deltaRadiusRange;
  parameterDeltaRadius.NumbersRange = deltaRadiusRange;
  parameterDeltaRadius.NumbersRange->SetNumberOfComponents(1);
  parameterDeltaRadius.NumbersRange->SetNumberOfValues(2);
  parameterDeltaRadius.NumbersRange->SetValue(0, 0.0);
  parameterDeltaRadius.NumbersRange->SetValue(1, 10.0);

  this->InputParameterInfo.push_back(parameterDeltaRadius);

  // Auxiliar plane source is used to create a plane for the input profile when it is a markups plane
  this->AuxiliarPlaneSource = vtkSmartPointer<vtkPlaneSource>::New();
  
  this->InputProfileToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputProfileNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputProfileToWorldTransformFilter->SetTransform(this->InputProfileNodeToWorldTransform);

  this->WorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->WorldToModelTransform = vtkSmartPointer<vtkTransform>::New();
  this->WorldToModelTransform->PostMultiply();
  this->WorldToModelTransformFilter->SetTransform(this->WorldToModelTransform);

  this->CapTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->CapTransform = vtkSmartPointer<vtkTransform>::New();
  this->CapTransform->PostMultiply();
  this->CapTransformFilter->SetTransform(this->CapTransform);

  this->BoundaryEdgesFilter = vtkSmartPointer<vtkFeatureEdges>::New();
  this->BoundaryEdgesFilter->BoundaryEdgesOn();
  this->BoundaryEdgesFilter->FeatureEdgesOff();
  this->BoundaryEdgesFilter->NonManifoldEdgesOff();
  this->BoundaryEdgesFilter->ManifoldEdgesOff();
  this->BoundaryEdgesFilter->PassLinesOn();

  this->RevolveFilter = vtkSmartPointer<vtkRotationalExtrusionFilter>::New();
  this->RevolveFilter->SetInputConnection(this->BoundaryEdgesFilter->GetOutputPort());

  this->AppendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

  this->ModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->ModelToWorldTransform = vtkSmartPointer<vtkTransform>::New();
  this->ModelToWorldTransform->PostMultiply();
  this->ModelToWorldTransformFilter->SetTransform(this->ModelToWorldTransform);

  this->OutputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputModelToWorldTransformFilter->SetTransform(this->OutputWorldToModelTransform);
  this->OutputModelToWorldTransformFilter->SetInputConnection(this->RevolveFilter->GetOutputPort());
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

  vtkMRMLModelNode* inputProfileModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsNode* inputProfileMarkupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsCurveNode* inputProfileMarkupsCurveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsClosedCurveNode* inputProfileMarkupsClosedCurveNode = vtkMRMLMarkupsClosedCurveNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsFiducialNode* inputProfileMarkupsPointsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsLineNode* inputProfileMarkupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsPlaneNode* inputProfileMarkupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  vtkMRMLMarkupsAngleNode* inputProfileMarkupsAngleNode = vtkMRMLMarkupsAngleNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_PROFILE_REFERENCE_ROLE));
  if ((!inputProfileModelNode) && (!inputProfileMarkupsNode))
  {
    vtkErrorMacro("Invalid input node!");
    return false;
  }

  if (inputProfileModelNode)
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
  else // inputProfileMarkupsNode
  {
    this->InputProfileNodeToWorldTransform->Identity(); // this way the transformFilter is a pass-through
    if (inputProfileMarkupsPlaneNode)
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
    else if (
      inputProfileMarkupsPointsNode || 
      inputProfileMarkupsLineNode || 
      inputProfileMarkupsAngleNode ||
      inputProfileMarkupsCurveNode ||
      inputProfileMarkupsClosedCurveNode
    )
    {
      if (!inputProfileMarkupsNode->GetCurveWorld() || inputProfileMarkupsNode->GetCurveWorld()->GetNumberOfPoints() == 0)
      {
        vtkNew<vtkPolyData> outputPolyData;
        outputModelNode->SetAndObservePolyData(outputPolyData);
        return true;
      }
      else
      {
        this->InputProfileToWorldTransformFilter->SetInputConnection(inputProfileMarkupsNode->GetCurveWorldConnection());
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


  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(surfaceEditorNode->GetNodeReference(REVOLVE_INPUT_MARKUPS_REFERENCE_ROLE));
  if (!this->inputMarkupIsValid(markupsNode))
  {
    vtkNew<vtkPolyData> outputPolyData;
    outputModelNode->SetAndObservePolyData(outputPolyData);
    return true;
  }

  vtkMRMLMarkupsFiducialNode* markupsFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsLineNode* markupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsPlaneNode* markupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsAngleNode* markupsAngleNode = vtkMRMLMarkupsAngleNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsCurveNode* markupsCurveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsClosedCurveNode* markupsClosedCurveNode = vtkMRMLMarkupsClosedCurveNode::SafeDownCast(markupsNode);
  int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();

  // get parameters
  double rotationAngleDegrees = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();
  bool axisIsAtOrigin = vtkVariant(surfaceEditorNode->GetAttribute(REVOLVE_AXIS_IS_AT_ORIGIN)).ToInt() != 0;
  double translationDistanceAlongAxis = this->GetNthInputParameterValue(2, surfaceEditorNode).ToDouble();
  double deltaRadius = this->GetNthInputParameterValue(3, surfaceEditorNode).ToDouble();

  this->RevolveFilter->SetResolution(
    std::ceil(std::fabs(rotationAngleDegrees))*2);
  this->RevolveFilter->SetAngle(rotationAngleDegrees); // redefined below if angle markup
  this->RevolveFilter->SetDeltaRadius(deltaRadius);
  this->RevolveFilter->SetTranslation(translationDistanceAlongAxis);

  // calculate the origin, axis
  double origin[3] = {0.,0.,0.};
  double axis[3] = {0.,0.,0.};
  if (markupsFiducialNode)
  {
    markupsFiducialNode->GetNthControlPointPositionWorld(0, origin);
    axis[2]= 1.0; // superior direction
  }
  if (markupsLineNode)
  {
    double endPoint[3] = {0.,0.,0.};
    markupsLineNode->GetNthControlPointPositionWorld(1, endPoint);
    markupsLineNode->GetNthControlPointPositionWorld(0, origin);
    vtkMath::Subtract(endPoint, origin, axis);
    vtkMath::Normalize(axis);
  }
  if (markupsPlaneNode)
  {
    markupsPlaneNode->GetNthControlPointPositionWorld(0, origin);
    markupsPlaneNode->GetNormalWorld(axis);
  }
  if (markupsCurveNode || markupsClosedCurveNode)
  {
    // get control points in world coordinates
    vtkNew<vtkPoints> controlPointsWorld;
    for (int i = 0; i < numberOfControlPoints; ++i)
    {
      double controlPointWorld[3] = { 0, 0, 0 };
      markupsNode->GetNthControlPointPositionWorld(i, controlPointWorld);
      controlPointsWorld->InsertNextPoint(controlPointWorld);
    }
    vtkPlane::ComputeBestFittingPlane(controlPointsWorld, origin, axis);
  }
  if (markupsAngleNode)
  {
    double firstPoint[3] = {0.,0.,0.};
    double thirdPoint[3] = {0.,0.,0.};
    markupsAngleNode->GetNthControlPointPositionWorld(0, firstPoint);
    markupsAngleNode->GetNthControlPointPositionWorld(1, origin);
    markupsAngleNode->GetNthControlPointPositionWorld(2, thirdPoint);
    double vector1[3] = {0.,0.,0.};
    double vector2[3] = {0.,0.,0.};
    vtkMath::Subtract(firstPoint, origin, vector1);
    vtkMath::Subtract(thirdPoint, origin, vector2);
    vtkMath::Normalize(vector1);
    vtkMath::Normalize(vector2);
    vtkMath::Cross(vector1,vector2,axis);
    vtkMath::Normalize(axis);

    double rotationAngleRadians = vtkMath::AngleBetweenVectors(vector1,vector2);
    rotationAngleDegrees = vtkMath::DegreesFromRadians(rotationAngleRadians);
    this->RevolveFilter->SetAngle(rotationAngleDegrees);
  }

  this->RevolveFilter->SetRotationAxis(axis);


  // some linear algebra is needed to calculate the final position of the cap when deltaRadius is not zero
  double rightDir[3] = {1.,0.,0.};
  double anteriorDir[3] = {0.,1.,0.};
  double superiorDir[3] = {0.,0.,1.};
  double projX[3] = {0.,0.,0.};
  double projY[3] = {0.,0.,0.};
  double projZ[3] = {0.,0.,0.};
  vtkMath::ProjectVector(rightDir,axis,projX);
  vtkMath::ProjectVector(anteriorDir,axis,projY);
  vtkMath::ProjectVector(superiorDir,axis,projZ);

  // create vtk matrix 3x3
  vtkNew<vtkMatrix3x3> projectionMatrix;
  projectionMatrix->SetElement(0,0,projX[0]);
  projectionMatrix->SetElement(1,0,projX[1]);
  projectionMatrix->SetElement(2,0,projX[2]);
  projectionMatrix->SetElement(0,1,projY[0]);
  projectionMatrix->SetElement(1,1,projY[1]);
  projectionMatrix->SetElement(2,1,projY[2]);
  projectionMatrix->SetElement(0,2,projZ[0]);
  projectionMatrix->SetElement(1,2,projZ[1]);
  projectionMatrix->SetElement(2,2,projZ[2]);

  vtkNew<vtkMatrix3x3> identityMatrix;
  identityMatrix->Identity();

  vtkNew<vtkMatrix3x3> tempResultMatrix;
  for (int i=0; i<3; i++)
  {
    for (int j=0; j<3; j++)
    {
      tempResultMatrix->SetElement(i,j,
        identityMatrix->GetElement(i,j) +
        deltaRadius*identityMatrix->GetElement(i,j) -
        deltaRadius*projectionMatrix->GetElement(i,j)
      );
    }
  }

  vtkNew<vtkMatrix4x4> resultMatrix;
  // identity matrix
  resultMatrix->Identity();
  // copy elements of the 3x3 matrix to the 4x4 matrix
  for (int i=0; i<3; i++)
  {
    for (int j=0; j<3; j++)
    {
      resultMatrix->SetElement(i,j,tempResultMatrix->GetElement(i,j));
    }
  }

  // final position of the cap
  this->CapTransform->Identity();
  this->CapTransform->RotateWXYZ(rotationAngleDegrees,axis[0],axis[1],axis[2]);
  this->CapTransform->Translate(
    translationDistanceAlongAxis*axis[0],
    translationDistanceAlongAxis*axis[1],
    translationDistanceAlongAxis*axis[2]);
  this->CapTransform->Concatenate(resultMatrix);


  // translate to origin the mesh to revolve
  if (axisIsAtOrigin == false)
  {
    this->WorldToModelTransform->Identity();
    this->WorldToModelTransform->Translate(-origin[0],-origin[1],-origin[2]);
    this->WorldToModelTransformFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());
    this->BoundaryEdgesFilter->SetInputConnection(this->WorldToModelTransformFilter->GetOutputPort());
    this->CapTransformFilter->SetInputConnection(this->WorldToModelTransformFilter->GetOutputPort());
    this->AppendFilter->RemoveAllInputs();
    this->AppendFilter->AddInputConnection(this->WorldToModelTransformFilter->GetOutputPort());
    this->AppendFilter->AddInputConnection(this->RevolveFilter->GetOutputPort());
    this->AppendFilter->AddInputConnection(this->CapTransformFilter->GetOutputPort());
    this->ModelToWorldTransform->Identity();
    this->ModelToWorldTransform->Translate(origin[0],origin[1],origin[2]);
    this->ModelToWorldTransformFilter->SetInputConnection(this->AppendFilter->GetOutputPort());
    this->OutputModelToWorldTransformFilter->SetInputConnection(this->ModelToWorldTransformFilter->GetOutputPort());
  }
  else
  {
    this->BoundaryEdgesFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());
    this->CapTransformFilter->SetInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());
    this->AppendFilter->RemoveAllInputs();
    this->AppendFilter->AddInputConnection(this->InputProfileToWorldTransformFilter->GetOutputPort());
    this->AppendFilter->AddInputConnection(this->RevolveFilter->GetOutputPort());
    this->AppendFilter->AddInputConnection(this->CapTransformFilter->GetOutputPort());
    this->OutputModelToWorldTransformFilter->SetInputConnection(this->AppendFilter->GetOutputPort());
  }
  
  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}

bool vtkSlicerDynamicModelerRevolveTool::inputMarkupIsValid(vtkMRMLMarkupsNode* markupsNode)
{
  vtkMRMLMarkupsFiducialNode* markupsFiducialNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsLineNode* markupsLineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsPlaneNode* markupsPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsAngleNode* markupsAngleNode = vtkMRMLMarkupsAngleNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsCurveNode* markupsCurveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(markupsNode);
  vtkMRMLMarkupsClosedCurveNode* markupsClosedCurveNode = vtkMRMLMarkupsClosedCurveNode::SafeDownCast(markupsNode);
  int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();
  bool validFiducial = ((markupsFiducialNode) && (numberOfControlPoints >= 1));
  bool validLine = ((markupsLineNode) && (numberOfControlPoints == 2));
  bool validPlane = ((markupsPlaneNode) && (markupsPlaneNode->GetIsPlaneValid()));
  bool validAngle = ((markupsAngleNode) && (numberOfControlPoints == 3));
  bool validCurve = ((markupsCurveNode) && (numberOfControlPoints >= 3));
  bool validClosedCurve = ((markupsClosedCurveNode) && (numberOfControlPoints >= 3));

  if (validFiducial || validLine || validPlane || validAngle || validCurve || validClosedCurve)
  {
    return true;
  }
  else
  {
    return false;
  }
}