#include "vtkMRMLDynamicModelerNode.h"
#include "vtkSlicerDynamicModelerPlaneCutTool.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkFeatureEdges.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
vtkSmartPointer<vtkPolyData> CreateTorus()
{
  constexpr int ringResolution = 128;
  constexpr int crossSectionResolution = 64;
  constexpr double ringRadius = 12.0;
  constexpr double crossSectionRadius = 4.0;

  vtkNew<vtkPoints> points;
  for (int ringIndex = 0; ringIndex < ringResolution; ++ringIndex)
    {
    double ringAngle = 2.0 * vtkMath::Pi() * ringIndex / ringResolution;
    for (int crossSectionIndex = 0; crossSectionIndex < crossSectionResolution; ++crossSectionIndex)
      {
      double crossSectionAngle = 2.0 * vtkMath::Pi() * crossSectionIndex / crossSectionResolution;
      double radialDistance = ringRadius + crossSectionRadius * std::cos(crossSectionAngle);
      points->InsertNextPoint(
        radialDistance * std::cos(ringAngle),
        radialDistance * std::sin(ringAngle),
        crossSectionRadius * std::sin(crossSectionAngle));
      }
    }

  vtkNew<vtkCellArray> triangles;
  for (int ringIndex = 0; ringIndex < ringResolution; ++ringIndex)
    {
    int nextRingIndex = (ringIndex + 1) % ringResolution;
    for (int crossSectionIndex = 0; crossSectionIndex < crossSectionResolution; ++crossSectionIndex)
      {
      int nextCrossSectionIndex = (crossSectionIndex + 1) % crossSectionResolution;
      vtkIdType point00 = ringIndex * crossSectionResolution + crossSectionIndex;
      vtkIdType point10 = nextRingIndex * crossSectionResolution + crossSectionIndex;
      vtkIdType point01 = ringIndex * crossSectionResolution + nextCrossSectionIndex;
      vtkIdType point11 = nextRingIndex * crossSectionResolution + nextCrossSectionIndex;
      vtkIdType triangle1[3] = { point00, point10, point11 };
      vtkIdType triangle2[3] = { point00, point11, point01 };
      triangles->InsertNextCell(3, triangle1);
      triangles->InsertNextCell(3, triangle2);
      }
    }

  vtkSmartPointer<vtkPolyData> torus = vtkSmartPointer<vtkPolyData>::New();
  torus->SetPoints(points);
  torus->SetPolys(triangles);
  return torus;
}

bool IsClosedSurface(vtkPolyData* polyData, const char* outputName)
{
  vtkNew<vtkCleanPolyData> cleaner;
  cleaner->SetInputData(polyData);
  cleaner->PointMergingOn();
  cleaner->ToleranceIsAbsoluteOn();
  cleaner->SetAbsoluteTolerance(1e-6);

  vtkNew<vtkFeatureEdges> boundaryEdges;
  boundaryEdges->SetInputConnection(cleaner->GetOutputPort());
  boundaryEdges->BoundaryEdgesOn();
  boundaryEdges->NonManifoldEdgesOff();
  boundaryEdges->FeatureEdgesOff();
  boundaryEdges->ManifoldEdgesOff();
  boundaryEdges->Update();

  vtkNew<vtkFeatureEdges> nonManifoldEdges;
  nonManifoldEdges->SetInputConnection(cleaner->GetOutputPort());
  nonManifoldEdges->BoundaryEdgesOff();
  nonManifoldEdges->NonManifoldEdgesOn();
  nonManifoldEdges->FeatureEdgesOff();
  nonManifoldEdges->ManifoldEdgesOff();
  nonManifoldEdges->Update();

  vtkIdType numberOfBoundaryEdges = boundaryEdges->GetOutput()->GetNumberOfCells();
  vtkIdType numberOfNonManifoldEdges = nonManifoldEdges->GetOutput()->GetNumberOfCells();
  if (numberOfBoundaryEdges != 0 || numberOfNonManifoldEdges != 0)
    {
    std::cerr << outputName << " has " << numberOfBoundaryEdges << " boundary edges and "
              << numberOfNonManifoldEdges << " non-manifold edges" << std::endl;
    return false;
    }
  return true;
}
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModelerPlaneCutToolTest(int, char*[])
{
  vtkSmartPointer<vtkPolyData> inputPolyData = CreateTorus();
  if (!IsClosedSurface(inputPolyData, "Input torus"))
    {
    return EXIT_FAILURE;
    }

  vtkNew<vtkMRMLScene> scene;

  vtkNew<vtkMRMLModelNode> inputModelNode;
  scene->AddNode(inputModelNode);
  inputModelNode->SetAndObservePolyData(inputPolyData);

  vtkNew<vtkMRMLMarkupsPlaneNode> planeNode;
  scene->AddNode(planeNode);
  planeNode->SetOrigin(0.0, 0.0, 0.35);
  planeNode->SetNormal(0.0, 0.0, 1.0);

  vtkNew<vtkMRMLMarkupsPlaneNode> secondPlaneNode;
  scene->AddNode(secondPlaneNode);
  secondPlaneNode->SetOrigin(1.1, 0.0, 0.0);
  secondPlaneNode->SetNormal(1.0, 0.2, 0.0);

  vtkNew<vtkMRMLModelNode> outputPositiveModelNode;
  scene->AddNode(outputPositiveModelNode);
  vtkNew<vtkMRMLModelNode> outputNegativeModelNode;
  scene->AddNode(outputNegativeModelNode);

  vtkNew<vtkMRMLDynamicModelerNode> dynamicModelerNode;
  scene->AddNode(dynamicModelerNode);
  dynamicModelerNode->SetNodeReferenceID("PlaneCut.InputModel", inputModelNode->GetID());
  dynamicModelerNode->AddNodeReferenceID("PlaneCut.InputPlane", planeNode->GetID());
  dynamicModelerNode->SetNodeReferenceID("PlaneCut.OutputPositiveModel", outputPositiveModelNode->GetID());
  dynamicModelerNode->SetNodeReferenceID("PlaneCut.OutputNegativeModel", outputNegativeModelNode->GetID());
  dynamicModelerNode->SetAttribute("CapSurface", "1");

  vtkNew<vtkSlicerDynamicModelerPlaneCutTool> planeCutTool;
  auto runAndCheckPlaneCut = [&](const char* operationType)
    {
    dynamicModelerNode->SetAttribute("OperationType", operationType);
    if (!planeCutTool->RunInternal(dynamicModelerNode))
      {
      std::cerr << operationType << " plane cut failed to run" << std::endl;
      return false;
      }

    vtkPolyData* positiveOutput = outputPositiveModelNode->GetPolyData();
    vtkPolyData* negativeOutput = outputNegativeModelNode->GetPolyData();
    if (!positiveOutput || positiveOutput->GetNumberOfPolys() == 0
      || !negativeOutput || negativeOutput->GetNumberOfPolys() == 0)
      {
      std::cerr << operationType << " plane cut produced an empty output" << std::endl;
      return false;
      }

    bool positiveOutputIsClosed = IsClosedSurface(positiveOutput, "Capped positive output");
    bool negativeOutputIsClosed = IsClosedSurface(negativeOutput, "Capped negative output");
    if (!positiveOutputIsClosed || !negativeOutputIsClosed)
      {
      std::cerr << "Plane cut operation: " << operationType << std::endl;
      return false;
      }
    return true;
    };

  if (!runAndCheckPlaneCut("Union"))
    {
    return EXIT_FAILURE;
    }

  dynamicModelerNode->AddNodeReferenceID("PlaneCut.InputPlane", secondPlaneNode->GetID());
  const char* operationTypes[] = { "Union", "Intersection", "Difference" };
  for (const char* operationType : operationTypes)
    {
    if (!runAndCheckPlaneCut(operationType))
      {
      return EXIT_FAILURE;
      }
    }

  vtkSmartPointer<vtkPolyData> shiftedInputPolyData = CreateTorus();
  for (vtkIdType pointIndex = 0; pointIndex < shiftedInputPolyData->GetNumberOfPoints(); ++pointIndex)
    {
    double point[3] = { 0.0 };
    shiftedInputPolyData->GetPoint(pointIndex, point);
    point[0] += 50.0;
    shiftedInputPolyData->GetPoints()->SetPoint(pointIndex, point);
    }
  inputModelNode->SetAndObservePolyData(shiftedInputPolyData);
  dynamicModelerNode->SetAttribute("CapSurface", "0");
  if (!planeCutTool->RunInternal(dynamicModelerNode))
    {
    std::cerr << "Uncapped plane cut failed to run after a capped cut" << std::endl;
    return EXIT_FAILURE;
    }

  double outputBounds[6] = { 0.0 };
  outputPositiveModelNode->GetPolyData()->GetBounds(outputBounds);
  if (outputBounds[0] < 30.0)
    {
    std::cerr << "Plane cut reused input geometry from the previous run" << std::endl;
    return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}