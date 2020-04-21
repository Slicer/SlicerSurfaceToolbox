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

#include "vtkSlicerParametricSurfacePlaneCutRule.h"

#include "vtkMRMLParametricSurfaceEditorNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkClipPolyData.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkDataSetAttributes.h>
#include <vtkFeatureEdges.h>
#include <vtkGeneralTransform.h>
#include <vtkGeometryFilter.h>
#include <vtkIntArray.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStripper.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkRuleNewMacro(vtkSlicerParametricSurfacePlaneCutRule);

//----------------------------------------------------------------------------
vtkSlicerParametricSurfacePlaneCutRule::vtkSlicerParametricSurfacePlaneCutRule()
{
  /////////
  // Inputs
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
    "PlaneCut.InputPlane",
    true,
    inputPlaneEvents
    );
  this->InputNodeInfo.push_back(inputPlane);

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
    "PlaneCut.InputModel",
    true,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputPositiveModel(
    "Clipped output model (positive side)",
    "Portion of the cut model that is on the same side of the plane as the normal.",
    inputModelClassNames,
    "PlaneCut.OutputPositiveModel",
    false
    );
  this->OutputNodeInfo.push_back(outputPositiveModel);

  NodeInfo outputNegativeModel(
    "Clipped output model (negative side)",
    "Portion of the cut model that is on the opposite side of the plane as the normal.",
    inputModelClassNames,
    "PlaneCut.OutputNegativeModel",
    false
    );
  this->OutputNodeInfo.push_back(outputNegativeModel);

  /////////
  // Parameters
  ParameterInfo parameterCapSurface(
    "Cap surface",
    "Create a closed surface by triangulating the clipped region",
    "CapSurface",
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterCapSurface);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->PlaneClipper = vtkSmartPointer<vtkClipClosedSurface>::New();
  this->PlaneClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->PlaneClipper->SetClippingPlanes(vtkNew <vtkPlaneCollection>());
  this->PlaneClipper->SetScalarModeToLabels();
  this->PlaneClipper->TriangulationErrorDisplayOff();
  
  this->Plane = vtkSmartPointer<vtkPlane>::New();
  this->PlaneClipper->GetClippingPlanes()->AddItem(this->Plane);

  this->ThresholdFilter = vtkSmartPointer<vtkThreshold>::New();
  this->ThresholdFilter->SetInputConnection(this->PlaneClipper->GetOutputPort());
  this->ThresholdFilter->ThresholdByLower(0);
  this->ThresholdFilter->SetInputArrayToProcess(0, 0, 0,
    vtkDataObject::FIELD_ASSOCIATION_CELLS,
    vtkDataSetAttributes::SCALARS);

  this->GeometryFilter = vtkSmartPointer<vtkGeometryFilter>::New();
  this->GeometryFilter->SetInputConnection(this->ThresholdFilter->GetOutputPort());

  this->OutputPositiveModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputPositiveWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputPositiveModelToWorldTransformFilter->SetTransform(this->OutputPositiveWorldToModelTransform);
  this->OutputPositiveModelToWorldTransformFilter->SetInputConnection(this->PlaneClipper->GetOutputPort());

  this->OutputNegativeModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputNegativeWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputNegativeModelToWorldTransformFilter->SetTransform(this->OutputNegativeWorldToModelTransform);
  this->OutputNegativeModelToWorldTransformFilter->SetInputConnection(this->PlaneClipper->GetOutputPort());
}

//----------------------------------------------------------------------------
vtkSlicerParametricSurfacePlaneCutRule::~vtkSlicerParametricSurfacePlaneCutRule()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerParametricSurfacePlaneCutRule::GetName()
{
  return "Plane cut";
}

//----------------------------------------------------------------------------
void vtkSlicerParametricSurfacePlaneCutRule::CreateEndCap(vtkPolyData* polyData)
{
  // Now extract feature edges
  vtkNew<vtkFeatureEdges> boundaryEdges;
  boundaryEdges->SetInputData(polyData);
  boundaryEdges->BoundaryEdgesOn();
  boundaryEdges->FeatureEdgesOff();
  boundaryEdges->NonManifoldEdgesOff();
  boundaryEdges->ManifoldEdgesOff();
  
  vtkNew<vtkStripper> boundaryStrips;
  boundaryStrips->SetInputConnection(boundaryEdges->GetOutputPort());
  boundaryStrips->Update();
  
  vtkNew<vtkPolyData> boundaryPolyData;
  boundaryPolyData->SetPoints(boundaryStrips->GetOutput()->GetPoints());
  boundaryPolyData->SetPolys(boundaryStrips->GetOutput()->GetLines());

  vtkNew<vtkAppendPolyData> append;
  append->AddInputData(polyData);
  append->AddInputData(boundaryPolyData);
  append->Update();
  polyData->DeepCopy(append->GetOutput());
}

//----------------------------------------------------------------------------
bool vtkSlicerParametricSurfacePlaneCutRule::RunInternal(vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputPositiveModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthOutputNode(0, surfaceEditorNode));
  vtkMRMLModelNode* outputNegativeModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthOutputNode(1, surfaceEditorNode));
  if (!outputPositiveModelNode && !outputNegativeModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLMarkupsPlaneNode* inputPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(this->GetNthInputNode(0, surfaceEditorNode));
  vtkMRMLSliceNode* inputSliceNode = vtkMRMLSliceNode::SafeDownCast(this->GetNthInputNode(0, surfaceEditorNode));
  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthInputNode(1, surfaceEditorNode));
  if ((!inputSliceNode && !inputPlaneNode) || !inputModelNode)
    {
    vtkErrorMacro("Invalid input nodes!");
    return false;
    }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0 || (inputPlaneNode && inputPlaneNode->GetNumberOfControlPoints() < 3))
    {
    inputModelNode->GetMesh()->Initialize();
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
  this->Plane->SetNormal(normal_World);
  this->Plane->SetOrigin(origin_World);

  bool capSurface = this->GetNthInputParameterValue(0, surfaceEditorNode).ToInt() != 0;
  if (capSurface)
    {
    this->OutputPositiveModelToWorldTransformFilter->SetInputConnection(this->PlaneClipper->GetOutputPort());
    this->OutputNegativeModelToWorldTransformFilter->SetInputConnection(this->PlaneClipper->GetOutputPort());
    }
  else
    {
    this->OutputPositiveModelToWorldTransformFilter->SetInputConnection(this->GeometryFilter->GetOutputPort());
    this->OutputNegativeModelToWorldTransformFilter->SetInputConnection(this->GeometryFilter->GetOutputPort());
    }

  if (outputPositiveModelNode)
    {
    this->OutputPositiveModelToWorldTransformFilter->Update();
    vtkNew<vtkPolyData> outputMesh;
    outputMesh->DeepCopy(this->OutputPositiveModelToWorldTransformFilter->GetOutput());

    MRMLNodeModifyBlocker blocker(outputPositiveModelNode);
    outputPositiveModelNode->SetAndObserveMesh(outputMesh);
    outputPositiveModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  if (outputNegativeModelNode)
    {
    vtkMath::MultiplyScalar(normal_World, -1.0);
    this->Plane->SetNormal(normal_World);
    this->Plane->SetOrigin(origin_World);

    this->OutputNegativeModelToWorldTransformFilter->Update();
    vtkNew<vtkPolyData> outputMesh;
    outputMesh->DeepCopy(this->OutputNegativeModelToWorldTransformFilter->GetOutput());

    MRMLNodeModifyBlocker blocker(outputNegativeModelNode);
    outputNegativeModelNode->SetAndObserveMesh(outputMesh);
    outputNegativeModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  return true;
}