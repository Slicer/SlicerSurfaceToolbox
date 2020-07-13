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

#include "vtkSlicerDynamicModelerBoundaryCutTool.h"

// MRML includes
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkCellData.h>
#include <vtkCellLocator.h>
#include <vtkCleanPolyData.h>
#include <vtkClipPolyData.h>
#include <vtkCommand.h>
#include <vtkConnectivityFilter.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkFeatureEdges.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPointLocator.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStripper.h>
#include <vtkTransformPolyDataFilter.h>

// DynamicModelerLogic includes
#include "vtkImplicitPolyDataPointDistance.h"

// DynamicModelerMRML includes
#include "vtkMRMLDynamicModelerNode.h"

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerBoundaryCutTool);

const char* INPUT_BORDER_REFERENCE_ROLE = "BoundaryCut.InputBorder";
const char* INPUT_MODEL_REFERENCE_ROLE = "BoundaryCut.InputModel";
const char* INPUT_SEED_REFERENCE_ROLE = "BoundaryCut.InputSeed";
const char* OUTPUT_MODEL_REFERENCE_ROLE = "BoundaryCut.OutputModel";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerBoundaryCutTool::vtkSlicerDynamicModelerBoundaryCutTool()
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
    "Model node to be cut with the curve.",
    inputModelClassNames,
    INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkIntArray> inputMarkupEvents;
  inputMarkupEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);

  vtkNew<vtkStringArray> inputBorderClassNames;
  inputBorderClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  inputBorderClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  NodeInfo inputBorder(
    "Border node",
    "Markup node that creates part of the border for the region that will be extracted.",
    inputBorderClassNames,
    INPUT_BORDER_REFERENCE_ROLE,
    true,
    true,
    inputMarkupEvents
    );
  this->InputNodeInfo.push_back(inputBorder);

  vtkNew<vtkStringArray> inputSeedFiducialClassNames;
  inputSeedFiducialClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  NodeInfo inputSeed(
    "Seed fiducial node",
    "Markup fiducial node that designates the region from the surface that should be preserved.",
    inputSeedFiducialClassNames,
    INPUT_SEED_REFERENCE_ROLE,
    false,
    false,
    inputMarkupEvents
  );
  this->InputNodeInfo.push_back(inputSeed);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Model node",
    "Output model containing the cut region.",
    inputModelClassNames,
    OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  this->InputModelToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelToWorldTransform);

  double epsilon = 1e-5;
  this->ClipPolyData = vtkSmartPointer<vtkClipPolyData>::New();
  this->ClipPolyData->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->ClipPolyData->SetValue(epsilon);
  this->ClipPolyData->InsideOutOn();
  this->ClipPolyData->GenerateClippedOutputOn();

  this->Connectivity = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
  this->Connectivity->SetInputConnection(this->ClipPolyData->GetClippedOutputPort());
  this->Connectivity->SetExtractionModeToPointSeededRegions();

  this->ColorConnectivity = vtkSmartPointer<vtkConnectivityFilter>::New();
  this->ColorConnectivity->ColorRegionsOn();
  this->ColorConnectivity->SetExtractionModeToAllRegions();
  this->ColorConnectivity->SetInputConnection(this->Connectivity->GetOutputPort());

  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransformFilter->SetInputConnection(this->ColorConnectivity->GetOutputPort());
  this->OutputWorldToModelTransformFilter->SetTransform(this->OutputWorldToModelTransform);

  this->ClippedModelPointLocator = vtkSmartPointer<vtkPointLocator>::New();
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerBoundaryCutTool::~vtkSlicerDynamicModelerBoundaryCutTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerBoundaryCutTool::GetName()
{
  return "BoundaryCut";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerBoundaryCutTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(INPUT_MODEL_REFERENCE_ROLE));
  if (!inputModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkPolyData* inputPolyData = inputModelNode->GetPolyData();
  if (!inputPolyData || inputPolyData->GetNumberOfPoints() < 1)
    {
    // Nothing to output
    return true;
    }

  this->InputModelToWorldTransformFilter->SetInputData(inputPolyData);
  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelToWorldTransform);
    }
  else
    {
    this->InputModelToWorldTransform->Identity();
    }

  vtkNew<vtkAppendPolyData> appendFilter;
  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(INPUT_BORDER_REFERENCE_ROLE);
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(INPUT_BORDER_REFERENCE_ROLE, i);

    vtkNew<vtkPolyData> outputLinePolyData;
    vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
    if (planeNode)
      {
      if (planeNode->GetNumberOfControlPoints() < 3)
        {
        continue;
        }

      double normal_World[3] = { 0.0 };
      planeNode->GetNormalWorld(normal_World);

      double origin_World[3] = { 0.0 };
      planeNode->GetOriginWorld(origin_World);

      vtkNew<vtkPlane> plane;
      plane->SetNormal(normal_World);
      plane->SetOrigin(origin_World);

      vtkNew<vtkExtractPolyDataGeometry> planeExtractor;
      planeExtractor->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
      planeExtractor->SetImplicitFunction(plane);
      planeExtractor->ExtractInsideOff();
      planeExtractor->ExtractBoundaryCellsOff();

      vtkNew<vtkFeatureEdges> boundaryEdges;
      boundaryEdges->SetInputConnection(planeExtractor->GetOutputPort());
      boundaryEdges->BoundaryEdgesOn();
      boundaryEdges->FeatureEdgesOff();
      boundaryEdges->NonManifoldEdgesOff();
      boundaryEdges->ManifoldEdgesOff();

      vtkNew<vtkStripper> boundaryStrips;
      boundaryStrips->SetInputConnection(boundaryEdges->GetOutputPort());
      boundaryStrips->Update();

      outputLinePolyData->SetPoints(boundaryStrips->GetOutput()->GetPoints());
      outputLinePolyData->SetLines(boundaryStrips->GetOutput()->GetLines());
      }

    vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
    if (curveNode)
      {
      vtkPoints* curvePoints = curveNode->GetCurvePointsWorld();
      if (!curvePoints)
        {
        continue;
        }

      vtkNew<vtkIdList> line;
      for (int i = 0; i < curvePoints->GetNumberOfPoints(); ++i)
        {
        line->InsertNextId(i);
        }
      vtkNew<vtkCellArray> lines;
      lines->InsertNextCell(line);

      outputLinePolyData->SetPoints(curvePoints);
      outputLinePolyData->SetLines(lines);
      }
    appendFilter->AddInputData(outputLinePolyData);
    }
  vtkNew<vtkCleanPolyData> cleanFilter;
  cleanFilter->SetInputConnection(appendFilter->GetOutputPort());
  cleanFilter->Update();

  if (cleanFilter->GetOutput()->GetNumberOfPoints() < 1)
    {
    return false;
    }

  vtkNew<vtkImplicitPolyDataPointDistance> distance;
  distance->SetInput(cleanFilter->GetOutput());

  this->ClipPolyData->SetClipFunction(distance);
  this->ClipPolyData->Update();

  this->ClippedModelPointLocator->SetDataSet(this->ClipPolyData->GetOutput());
  this->ClippedModelPointLocator->BuildLocator();

  vtkNew<vtkPoints> seedPoints;
  this->GetSeedPoints(surfaceEditorNode, seedPoints);
  this->Connectivity->InitializeSeedList();
  for (int i = 0; i < seedPoints->GetNumberOfPoints(); ++i)
    {
    double* seedPoint = seedPoints->GetPoint(i);
    vtkIdType pointId = this->ClippedModelPointLocator->FindClosestPoint(seedPoint);
    this->Connectivity->AddSeed(pointId);
    }

  this->ColorOutputRegions(seedPoints);
  if (outputModelNode && outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
    }
  this->OutputWorldToModelTransformFilter->Update();

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputPolyData);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerBoundaryCutTool::ColorOutputRegions(vtkPoints* seedPoints)
{
  this->ColorConnectivity->Update();

  vtkPolyData* coloredPolyData = this->ColorConnectivity->GetPolyDataOutput();
  if (coloredPolyData)
    {
    coloredPolyData->GetPointData()->RemoveArray("RegionId"); // We only want the need the cell data color

    vtkIdTypeArray* regionArray = vtkIdTypeArray::SafeDownCast(coloredPolyData->GetCellData()->GetArray("RegionId"));
    if (regionArray)
      {
      std::map<vtkIdType, vtkIdType> regionMap;
      vtkNew<vtkCellLocator> cellLocator;
      cellLocator->SetDataSet(coloredPolyData);
      cellLocator->BuildLocator();

      // Determine what the scalar value should be for each of the input seed regions.
      // Scalar values should be fiducial index + 1.
      for (int i = 0; i < seedPoints->GetNumberOfPoints(); ++i)
        {
        double* seedPoint = seedPoints->GetPoint(i);
        vtkIdType cellId = -1;
        int subId;
        double dist2;
        double closestPoint[3];
        cellLocator->FindClosestPoint(seedPoint, closestPoint, cellId, subId, dist2);
        if (cellId > 0)
          {
          vtkIdType oldRegionId = regionArray->GetValue(cellId);
          vtkIdType newRegionId = i + 1;
          regionMap[oldRegionId] = newRegionId;
          }
        }

      // Replace values in the cell data array with the updated region ids
      long long* pointer = regionArray->GetPointer(0);
      for (int i = 0; i < regionArray->GetNumberOfValues(); ++i)
        {
        if (regionMap.find(*pointer) == regionMap.end())
          {
          *pointer = 0;
          }
        else
          {
          *pointer = regionMap[*pointer];
          }
        ++pointer;
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerBoundaryCutTool::GetSeedPoints(vtkMRMLDynamicModelerNode* surfaceEditorNode, vtkPoints* seedPoints)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("vtkSlicerDynamicModelerBoundaryCutTool::GetSeedPoints: Invalid surfaceEditorNode");
    return;
    }

  if (!seedPoints)
    {
    vtkErrorMacro("vtkSlicerDynamicModelerBoundaryCutTool::GetSeedPoints: Invalid seedPoints");
    return;
    }

  vtkMRMLMarkupsFiducialNode* seedNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(
    surfaceEditorNode->GetNthNodeReference(INPUT_SEED_REFERENCE_ROLE, 0));

  if (!seedNode || seedNode->GetNumberOfControlPoints() == 0)
    {
    double defaultSeedPoint[3] = { 0.0, 0.0, 0.0 };
    this->GetDefaultSeedPoint(surfaceEditorNode, defaultSeedPoint);
    seedPoints->InsertNextPoint(defaultSeedPoint);
    return;
    }

  for (int i = 0; i < seedNode->GetNumberOfControlPoints(); ++i)
    {
    double seedPoint[3] = { 0.0, 0.0, 0.0 };
    seedNode->GetNthControlPointPositionWorld(i, seedPoint);
    seedPoints->InsertNextPoint(seedPoint);
    }
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerBoundaryCutTool::GetDefaultSeedPoint(vtkMRMLDynamicModelerNode * surfaceEditorNode, double defaultSeedPoint[3])
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("GetDefaultSeedPoint::GetSeedPoints: Invalid surfaceEditorNode");
    return;
    }

  defaultSeedPoint[0] = 0.0;
  defaultSeedPoint[1] = 0.0;
  defaultSeedPoint[2] = 0.0;

  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(INPUT_BORDER_REFERENCE_ROLE);
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(INPUT_BORDER_REFERENCE_ROLE, i);
    double currentCenter_World[3] = { 0.0 };

    vtkNew<vtkPolyData> outputLinePolyData;
    vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
    if (planeNode)
      {
      planeNode->GetOriginWorld(currentCenter_World);
      }
    vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
    if (curveNode)
      {
      double inv_N = 1. / curveNode->GetNumberOfControlPoints();
      for (int i = 0; i < curveNode->GetNumberOfControlPoints(); ++i)
        {
        double p[4];
        curveNode->GetNthControlPointPositionWorld(i, p);
        currentCenter_World[0] += p[0] * inv_N;
        currentCenter_World[1] += p[1] * inv_N;
        currentCenter_World[2] += p[2] * inv_N;
        }
      }
    vtkMath::MultiplyScalar(currentCenter_World, 1.0 / numberOfInputNodes);
    vtkMath::Add(currentCenter_World, defaultSeedPoint, defaultSeedPoint);
    }
}
