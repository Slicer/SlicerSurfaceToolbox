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

// DynamicModeler Logic includes
#include "vtkSlicerDynamicModelerPlaneCutTool.h"
#include "vtkSlicerDynamicModelerROICutTool.h"

// DynamicModeler MRML includes
#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
//#include <vtkClipPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkImplicitBoolean.h>
#include <vtkPlane.h>
#include <vtkPlanes.h>
#include <vtkPlaneCollection.h>
#include <vtkReverseSense.h>
#include <vtkSmartPointer.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerROICutTool);

// Input references
const char* ROI_CUT_INPUT_MODEL_REFERENCE_ROLE = "ROICut.InputModel";
const char* ROI_CUT_INPUT_ROI_REFERENCE_ROLE = "ROICut.InputROI";

// Output references
const char* ROI_CUT_OUTPUT_INSIDE_MODEL_REFERENCE_ROLE = "ROICut.OutputPositiveModel";
const char* ROI_CUT_OUTPUT_OUTSIDE_MODEL_REFERENCE_ROLE = "ROICut.OutputNegativeModel";

// Parameters
const char* ROI_CUT_CAP_SURFACE_ATTRIBUTE_NAME = "ROICut.CapSurface";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerROICutTool::vtkSlicerDynamicModelerROICutTool()
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
    this->GetInputModelReferenceRole(),
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputROIEvents;
  inputROIEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputROIEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputROIEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputROIClassNames;
  inputROIClassNames->InsertNextValue("vtkMRMLMarkupsROINode");
  NodeInfo inputROI(
    "ROI node",
    "ROI node to cut the model node.",
    inputROIClassNames,
    this->GetInputROIReferenceRole(),
    true,
    false,
    inputROIEvents
    );
  this->InputNodeInfo.push_back(inputROI);

  /////////
  // Outputs
  NodeInfo outputPositiveModel(
    "Clipped output model (inside)",
    "Portion of the cut model that is inside the ROI.",
    inputModelClassNames,
    this->GetOutputInsideModelReferenceRole(),
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputPositiveModel);

  NodeInfo outputNegativeModel(
    "Clipped output model (outside)",
    "Portion of the cut model that is outside the ROI.",
    inputModelClassNames,
    this->GetOutputOutsideModelReferenceRole(),
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputNegativeModel);

  /////////
  // Parameters

  ParameterInfo parameterCapSurface(
    "Cap surface",
    "Create a closed surface by triangulating the clipped region",
    this->GetCapSurfaceAttributeName(),
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterCapSurface);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  //this->ROIClipper = vtkSmartPointer<vtkClipPolyData>::New();
  //this->ROIClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  this->ROIClipper = vtkSmartPointer<vtkClipClosedSurface>::New();
  this->ROIClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  this->OutputInsideWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputInsideWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputInsideWorldToModelTransformFilter->SetInputConnection(this->ROIClipper->GetOutputPort());
  this->OutputInsideWorldToModelTransformFilter->SetTransform(this->OutputInsideWorldToModelTransform);

  this->OutputOutsideWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputOutsideWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  //this->OutputOutsideWorldToModelTransformFilter->SetInputConnection(this->ROIClipper->GetClippedOutputPort());
  this->OutputOutsideWorldToModelTransformFilter->SetInputConnection(this->ROIClipper->GetOutputPort());
  this->OutputOutsideWorldToModelTransformFilter->SetTransform(this->OutputOutsideWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerROICutTool::~vtkSlicerDynamicModelerROICutTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerROICutTool::GetName()
{
  return "ROI cut";
}

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerROICutTool::GetInputModelReferenceRole()
{
  return ROI_CUT_INPUT_MODEL_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerROICutTool::GetInputROIReferenceRole()
{
  return ROI_CUT_INPUT_ROI_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerROICutTool::GetOutputInsideModelReferenceRole()
{
  return ROI_CUT_OUTPUT_INSIDE_MODEL_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerROICutTool::GetOutputOutsideModelReferenceRole()
{
  return ROI_CUT_OUTPUT_OUTSIDE_MODEL_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerROICutTool::GetCapSurfaceAttributeName()
{
  return ROI_CUT_CAP_SURFACE_ATTRIBUTE_NAME;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerROICutTool::RunInternal(vtkMRMLDynamicModelerNode* dynamicModelerNode)
{
  if (!this->HasRequiredInputs(dynamicModelerNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputInsideModelNode = vtkMRMLModelNode::SafeDownCast(dynamicModelerNode->GetNodeReference(this->GetOutputInsideModelReferenceRole()));
  vtkMRMLModelNode* outputOutsideModelNode = vtkMRMLModelNode::SafeDownCast(dynamicModelerNode->GetNodeReference(this->GetOutputOutsideModelReferenceRole()));
  if (!outputInsideModelNode && !outputOutsideModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(dynamicModelerNode->GetNodeReference(this->GetInputModelReferenceRole()));
  if (!inputModelNode)
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }
  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
    if (outputInsideModelNode && outputInsideModelNode->GetPolyData())
      {
      outputInsideModelNode->GetPolyData()->Initialize();
      }
    if (outputOutsideModelNode && outputOutsideModelNode->GetPolyData())
      {
      outputOutsideModelNode->GetPolyData()->Initialize();
      }
    return true;
    }

  vtkMRMLMarkupsROINode* roiNode = vtkMRMLMarkupsROINode::SafeDownCast(dynamicModelerNode->GetNodeReference(this->GetInputROIReferenceRole()));
  if (!roiNode)
    {
    if (outputInsideModelNode)
      {
      outputInsideModelNode->SetAndObservePolyData(vtkNew<vtkPolyData>());
      }
    if (outputOutsideModelNode)
      {
      outputOutsideModelNode->SetAndObservePolyData(vtkNew<vtkPolyData>());
      }
    return true;
    }

  vtkNew<vtkPlanes> planes;
  roiNode->GetPlanesWorld(planes);

  //vtkNew<vtkImplicitBoolean> planeFunction;
  //planeFunction->SetOperationTypeToUnion();

  vtkNew<vtkPlaneCollection> planeCollection;
  for (int i = 0; i < planes->GetNumberOfPlanes(); ++i)
    {
    vtkPlane* currentPlane = planes->GetPlane(i);

    // vtkClipPolyData will remove the inside of the ROI (negative implicit function), we need to invert the planes so that the correct
    // region is preserved.
    double normal[3] = { 1,0,0 };
    currentPlane->GetNormal(normal);
    vtkMath::MultiplyScalar(normal, -1.0);

    double origin[3] = { 0.0, 0.0, 0.0 };
    currentPlane->GetOrigin(origin);

    // vtkPlaneCollection always returns a pointer to the same vtkPlane, so we need to make a copy.
    vtkNew<vtkPlane> plane;
    plane->SetNormal(normal);
    plane->SetOrigin(origin);

    //planeFunction->AddFunction(plane);
    planeCollection->AddItem(plane);
    }

  //this->ROIClipper->SetClipFunction(planeFunction);
  this->ROIClipper->SetClippingPlanes(planeCollection);
  //this->ROIClipper->SetGenerateClippedOutput(outputOutsideModelNode != nullptr);

  vtkMRMLTransformNode::GetTransformBetweenNodes(inputModelNode->GetParentTransformNode(), nullptr, this->InputModelNodeToWorldTransform);

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());
  this->InputModelToWorldTransformFilter->Update();

  bool capSurface = vtkVariant(dynamicModelerNode->GetAttribute(ROI_CUT_CAP_SURFACE_ATTRIBUTE_NAME)).ToInt() != 0;
  int roiType = roiNode->GetROIType();
  if (roiType != vtkMRMLMarkupsROINode::ROITypeBox && roiType != vtkMRMLMarkupsROINode::ROITypeBoundingBox)
    {
    capSurface = false;
    }

  this->ROIClipper->SetGenerateFaces(capSurface);
/*  vtkNew<vtkPolyData> endCapPolyData;
  if (capSurface)
    {
    vtkSlicerDynamicModelerPlaneCutTool::CreateEndCap(planeCollection, this->InputModelToWorldTransformFilter->GetOutput(), planeFunction, endCapPolyData);
    }
    */
  if (outputInsideModelNode)
    {
    vtkMRMLTransformNode::GetTransformBetweenNodes(nullptr, outputInsideModelNode->GetParentTransformNode(), this->OutputInsideWorldToModelTransform);
    this->OutputInsideWorldToModelTransformFilter->Update();

    MRMLNodeModifyBlocker blocker(outputInsideModelNode);
    vtkSmartPointer<vtkPolyData> outputMesh = outputInsideModelNode->GetPolyData();
    if (!outputMesh)
      {
      outputMesh = vtkSmartPointer<vtkPolyData>::New();
      outputInsideModelNode->SetAndObservePolyData(outputMesh);
      }
    outputMesh->DeepCopy(this->OutputInsideWorldToModelTransformFilter->GetOutput());
    /*if (capSurface)
      {
      vtkNew<vtkAppendPolyData> appendEndCap;
      appendEndCap->AddInputData(outputMesh);
      appendEndCap->AddInputData(endCapPolyData);
      appendEndCap->Update();
      outputMesh->ShallowCopy(appendEndCap->GetOutput());
      }*/
    outputInsideModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  if (outputOutsideModelNode)
    {
    vtkMRMLTransformNode::GetTransformBetweenNodes(nullptr, outputOutsideModelNode->GetParentTransformNode(), this->OutputOutsideWorldToModelTransform);
    this->OutputOutsideWorldToModelTransformFilter->Update();

    MRMLNodeModifyBlocker blocker(outputOutsideModelNode);
    vtkSmartPointer<vtkPolyData> outputMesh = outputOutsideModelNode->GetPolyData();
    if (!outputMesh)
      {
      outputMesh = vtkSmartPointer<vtkPolyData>::New();
      outputOutsideModelNode->SetAndObservePolyData(outputMesh);
      }
    outputMesh->DeepCopy(this->OutputOutsideWorldToModelTransformFilter->GetOutput());
    /*
    if (capSurface)
      {
      vtkNew<vtkReverseSense> reverseSense;
      reverseSense->SetInputData(endCapPolyData);
      reverseSense->ReverseCellsOn();
      reverseSense->ReverseNormalsOn();

      vtkNew<vtkAppendPolyData> appendEndCap;
      appendEndCap->AddInputData(outputMesh);
      appendEndCap->AddInputConnection(reverseSense->GetOutputPort());
      appendEndCap->Update();
      outputMesh->ShallowCopy(appendEndCap->GetOutput());
      }
      */
    outputOutsideModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  return true;
}
