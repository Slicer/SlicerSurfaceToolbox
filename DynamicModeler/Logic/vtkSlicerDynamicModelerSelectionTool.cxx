/*==============================================================================

==============================================================================*/

#include "vtkSlicerDynamicModelerSelectionTool.h"

#include "vtkMRMLDynamicModelerNode.h"

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

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerSelectionTool);

const char* SELECTION_INPUT_MODEL_REFERENCE_ROLE = "Selection.InputModel";
const char* SELECTION_INPUT_FIDUCIAL_LIST_REFERENCE_ROLE = "Selection.InputPlane";
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
    "Selection distance of model's faces to input fiducials.",
    "SelectionDistance",
    PARAMETER_DOUBLE,
    5.0);
  this->InputParameterInfo.push_back(parameterSelectionDistance);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->ModelDistanceToFiducialsFilter = vtkSmartPointer<vtkDistancePolyDataFilter>::New();
  this->ModelDistanceToFiducialsFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->ModelDistanceToFiducialsFilter->SignedDistanceOff()

  this->PlaneClipper = vtkSmartPointer<vtkClipPolyData>::New();
  this->PlaneClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->PlaneClipper->SetValue(0.0);

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

  double selectionDistance = this->GetNthInputParameterValue(0, surfaceEditorNode).ToDouble();

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());

  vtkNew<vtkAppendPolyData> appendFilter;
  for (int i = 0; i < fiducialNode->GetNumberOfControlPoints(); i++)
    {
    double position[3] = { 0.0, 0.0, 0.0};
    fiducialNode->GetNthControlPointPositionWorld(i,position);
    vtkNew<vtkSphereSource> sphere;
    sphere->SetRadius(0.1);
    sphere->SetCenter(position);
    sphere->SetThetaResolution(5);
    sphere->SetPhiResolution(5);
    sphere->Update();
    appendFilter->AddInputData(sphere->GetOutput());
    }

  appendFilter->Update();

  this->ModelDistanceToFiducialsFilter->SetInputData(0, this->InputModelToWorldTransformFilter->GetOutput());
  this->ModelDistanceToFiducialsFilter->SetInputData(1, appendFilter->GetOutput());
  this->ModelDistanceToFiducialsFilter->Update();

  if (outputSelectionScalarsModelNode)
    {
    //Do selectionScalarsModelProcessing

    vtkNew<vtkPolyData> outputMesh;
    outputMesh->ShallowCopy(this->ModelDistanceToFiducialsFilter->GetOutput());
    vtkCellData *cellScalars = outputMesh->GetCellData();
    vtkDoubleArray *distanceArray = cellScalars.GetArray("Distance");
    vtkNew<vtkIntArray> selectionArray;
    selectionArray->SetName("Selection")
    selectionArray->SetNumberOfValues(outputMesh->GetNumberOfCells())
    for (int i = 0; i < outputMesh->GetNumberOfCells(); i++)
      {
      if (distanceArray->GetTuple1(i) < selectionDistance)
        {
        selectionArray->SetTuple1(i,1)
        }
      else
        {
        selectionArray->SetTuple1(i,0)
        }
      }

    cellScalars->AddArray(selectionArray)

    this->OutputSelectionScalarsModelTransformFilter->SetInputData(outputMesh);
    this->OutputSelectionScalarsModelTransformFilter->Update();
    outputMesh->DeepCopy(this->OutputSelectionScalarsModelTransformFilter->GetOutput());

    MRMLNodeModifyBlocker blocker(outputSelectionScalarsModelNode);
    outputSelectionScalarsModelNode->SetAndObserveMesh(outputMesh);
    outputSelectionScalarsModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

    # Set up coloring by selection array
    vtkMRMLModelDisplayNode* outputDisplayNode = outputSelectionScalarsModelNode->GetDisplayNode();
    outputDisplayNode->SetActiveScalar("Selection", vtk::vtkAssignAttribute::CELL_DATA);
    outputDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeWarm1");
    outputDisplayNode->SetScalarVisibility(true);
    }

  if (outputSelectedFacesModelNode)
    {
    //Do selectedFacesModelProcessing

    vtkNew<vtkThreshold> thresholdFilter;
    thresholdFilter->SetInputData(this->ModelDistanceToFiducialsFilter->GetOutput());
    thresholdFilter->ThresholdBetween(0, selectionDistance);
    thresholdFilter->SetInputArrayToProcess(0, 0, 0,
        vtk::vtkDataObject::FIELD_ASSOCIATION_CELLS,
        "Distance");
    thresholdFilter->Update();
    vtkNew<vtkGeometryFilter> geometryFilter;
    geometryFilter->SetInputData(thresholdFilter->GetOutput());
    geometryFilter->Update();

    vtkNew<vtkPolyData> outputMesh;
    outputMesh->ShallowCopy(geometryFilter->GetOutput());

    this->OutputSelectedFacesModelTransformFilter->SetInputData(outputMesh);
    this->OutputSelectedFacesModelTransformFilter->Update();
    outputMesh->DeepCopy(this->OutputSelectedFacesModelTransformFilter->GetOutput());

    MRMLNodeModifyBlocker blocker(outputSelectedFacesModelNode);
    outputSelectedFacesModelNode->SetAndObserveMesh(outputMesh);
    outputSelectedFacesModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  return true;
}
