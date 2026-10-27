/*==============================================================================

  Program: 3D Slicer

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#include "vtkACVDRemeshFilter.h"

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkArrowSource.h>
#include <vtkCellArray.h>
#include <vtkCellArrayIterator.h>
#include <vtkCellLocator.h>
#include <vtkCylinderSource.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkReverseSense.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkStripper.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

namespace
{
//----------------------------------------------------------------------------
// Dot product of the normal of a triangle and its centroid. It is positive
// if the triangle faces away from the origin.
double OutwardMeasure(vtkPolyData* mesh, const vtkIdType* cellPointIds)
{
  double p0[3], p1[3], p2[3];
  mesh->GetPoint(cellPointIds[0], p0);
  mesh->GetPoint(cellPointIds[1], p1);
  mesh->GetPoint(cellPointIds[2], p2);
  double e0[3], e1[3], normal[3];
  vtkMath::Subtract(p1, p0, e0);
  vtkMath::Subtract(p2, p0, e1);
  vtkMath::Cross(e0, e1, normal);
  const double centroid[3] = { p0[0] + p1[0] + p2[0], p0[1] + p1[1] + p2[1], p0[2] + p1[2] + p2[2] };
  return vtkMath::Dot(normal, centroid);
}

//----------------------------------------------------------------------------
// Check that the output contains only triangles and, if requested, that all of
// them face away from the origin (the center of the input sphere).
bool CheckTriangles(vtkPolyData* mesh, const char* testName, bool checkOutward)
{
  if (mesh->GetNumberOfVerts() || mesh->GetNumberOfLines() || mesh->GetNumberOfStrips())
  {
    std::cerr << testName << ": output must only contain triangles" << std::endl;
    return false;
  }
  vtkSmartPointer<vtkCellArrayIterator> cellIterator = vtk::TakeSmartPointer(mesh->GetPolys()->NewIterator());
  for (cellIterator->GoToFirstCell(); !cellIterator->IsDoneWithTraversal(); cellIterator->GoToNextCell())
  {
    vtkIdType numberOfCellPoints = 0;
    const vtkIdType* cellPointIds = nullptr;
    cellIterator->GetCurrentCell(numberOfCellPoints, cellPointIds);
    if (numberOfCellPoints != 3)
    {
      std::cerr << testName << ": found a cell with " << numberOfCellPoints << " points" << std::endl;
      return false;
    }
    if (checkOutward && OutwardMeasure(mesh, cellPointIds) <= 0)
    {
      std::cerr << testName << ": triangle " << cellIterator->GetCurrentCellId() << " faces inward" << std::endl;
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
bool CheckNumberOfPoints(vtkPolyData* mesh, const char* testName, vtkIdType expected)
{
  if (mesh->GetNumberOfPoints() != expected)
  {
    std::cerr << testName << ": expected " << expected << " points, got " << mesh->GetNumberOfPoints() << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// Copy of the output, so that it is kept when the filter is updated again
vtkSmartPointer<vtkPolyData> Remesh(vtkACVDRemeshFilter* remesher)
{
  remesher->Update();
  vtkSmartPointer<vtkPolyData> output = vtkSmartPointer<vtkPolyData>::New();
  output->DeepCopy(remesher->GetOutput());
  return output;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> Sphere(int resolution)
{
  vtkNew<vtkSphereSource> sphere;
  sphere->SetThetaResolution(resolution);
  sphere->SetPhiResolution(resolution);
  sphere->Update();
  return sphere->GetOutput();
}

//----------------------------------------------------------------------------
// Number of triangles facing away from and toward the origin
void CountTriangleOrientations(vtkPolyData* mesh, vtkIdType& outward, vtkIdType& inward)
{
  outward = 0;
  inward = 0;
  vtkSmartPointer<vtkCellArrayIterator> cellIterator = vtk::TakeSmartPointer(mesh->GetPolys()->NewIterator());
  for (cellIterator->GoToFirstCell(); !cellIterator->IsDoneWithTraversal(); cellIterator->GoToNextCell())
  {
    vtkIdType numberOfCellPoints = 0;
    const vtkIdType* cellPointIds = nullptr;
    cellIterator->GetCurrentCell(numberOfCellPoints, cellPointIds);
    const double measure = OutwardMeasure(mesh, cellPointIds);
    if (measure > 0)
    {
      outward++;
    }
    else if (measure < 0)
    {
      inward++;
    }
  }
}

//----------------------------------------------------------------------------
// Standard deviation of the edge lengths divided by their mean. Lower values
// mean more uniform triangles.
double EdgeLengthCoefficientOfVariation(vtkPolyData* mesh)
{
  std::set<std::pair<vtkIdType, vtkIdType>> edges;
  vtkSmartPointer<vtkCellArrayIterator> cellIterator = vtk::TakeSmartPointer(mesh->GetPolys()->NewIterator());
  for (cellIterator->GoToFirstCell(); !cellIterator->IsDoneWithTraversal(); cellIterator->GoToNextCell())
  {
    vtkIdType numberOfCellPoints = 0;
    const vtkIdType* cellPointIds = nullptr;
    cellIterator->GetCurrentCell(numberOfCellPoints, cellPointIds);
    for (vtkIdType i = 0; i < numberOfCellPoints; ++i)
    {
      const vtkIdType a = cellPointIds[i];
      const vtkIdType b = cellPointIds[(i + 1) % numberOfCellPoints];
      edges.emplace(std::min(a, b), std::max(a, b));
    }
  }

  std::vector<double> lengths;
  for (const std::pair<vtkIdType, vtkIdType>& edge : edges)
  {
    double p0[3], p1[3];
    mesh->GetPoint(edge.first, p0);
    mesh->GetPoint(edge.second, p1);
    lengths.push_back(std::sqrt(vtkMath::Distance2BetweenPoints(p0, p1)));
  }
  double mean = 0;
  for (double length : lengths)
  {
    mean += length;
  }
  mean /= lengths.size();
  double variance = 0;
  for (double length : lengths)
  {
    variance += (length - mean) * (length - mean);
  }
  variance /= lengths.size();
  return std::sqrt(variance) / mean;
}

//----------------------------------------------------------------------------
// Smallest and largest distance from the points of a mesh to a surface
void DistanceRangeToSurface(vtkPolyData* mesh, vtkPolyData* surface, double range[2])
{
  vtkNew<vtkCellLocator> locator;
  locator->SetDataSet(surface);
  locator->BuildLocator();
  range[0] = VTK_DOUBLE_MAX;
  range[1] = 0;
  for (vtkIdType i = 0; i < mesh->GetNumberOfPoints(); ++i)
  {
    double point[3], closestPoint[3];
    mesh->GetPoint(i, point);
    vtkIdType cellId = 0;
    int subId = 0;
    double distance2 = 0;
    locator->FindClosestPoint(point, closestPoint, cellId, subId, distance2);
    range[0] = std::min(range[0], std::sqrt(distance2));
    range[1] = std::max(range[1], std::sqrt(distance2));
  }
}

//----------------------------------------------------------------------------
vtkIdType NumberOfUnusedPoints(vtkPolyData* mesh)
{
  std::vector<char> used(mesh->GetNumberOfPoints(), 0);
  vtkSmartPointer<vtkCellArrayIterator> cellIterator = vtk::TakeSmartPointer(mesh->GetPolys()->NewIterator());
  for (cellIterator->GoToFirstCell(); !cellIterator->IsDoneWithTraversal(); cellIterator->GoToNextCell())
  {
    vtkIdType numberOfCellPoints = 0;
    const vtkIdType* cellPointIds = nullptr;
    cellIterator->GetCurrentCell(numberOfCellPoints, cellPointIds);
    for (vtkIdType i = 0; i < numberOfCellPoints; ++i)
    {
      used[cellPointIds[i]] = 1;
    }
  }
  return static_cast<vtkIdType>(std::count(used.begin(), used.end(), 0));
}

//----------------------------------------------------------------------------
// Same points and same triangles, in the same order
bool SameMesh(vtkPolyData* a, vtkPolyData* b)
{
  if (a->GetNumberOfPoints() != b->GetNumberOfPoints() || a->GetNumberOfPolys() != b->GetNumberOfPolys())
  {
    return false;
  }
  for (vtkIdType i = 0; i < a->GetNumberOfPoints(); ++i)
  {
    double pa[3], pb[3];
    a->GetPoint(i, pa);
    b->GetPoint(i, pb);
    if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2])
    {
      return false;
    }
  }
  vtkSmartPointer<vtkCellArrayIterator> iteratorA = vtk::TakeSmartPointer(a->GetPolys()->NewIterator());
  vtkSmartPointer<vtkCellArrayIterator> iteratorB = vtk::TakeSmartPointer(b->GetPolys()->NewIterator());
  for (iteratorA->GoToFirstCell(), iteratorB->GoToFirstCell(); !iteratorA->IsDoneWithTraversal(); iteratorA->GoToNextCell(), iteratorB->GoToNextCell())
  {
    vtkIdType sizeA = 0, sizeB = 0;
    const vtkIdType* idsA = nullptr;
    const vtkIdType* idsB = nullptr;
    iteratorA->GetCurrentCell(sizeA, idsA);
    iteratorB->GetCurrentCell(sizeB, idsB);
    if (sizeA != sizeB || !std::equal(idsA, idsA + sizeA, idsB))
    {
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
bool TestClamping()
{
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetNumberOfClusters(0);
  remesher->SetNumberOfSubdivisions(-1);
  remesher->SetMaximumNumberOfIterations(-5);
  remesher->SetIsolatedClusterRemovalAttempts(-1);
  if (remesher->GetNumberOfClusters() != 1 || remesher->GetNumberOfSubdivisions() != 0 || remesher->GetMaximumNumberOfIterations() != 0
      || remesher->GetIsolatedClusterRemovalAttempts() != 0)
  {
    std::cerr << "Clamping: settings must be clamped to their valid range" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// Moving points between clusters makes the triangle sizes more uniform
bool TestMaximumNumberOfIterations()
{
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputData(Sphere(100));
  remesher->SetNumberOfClusters(500);
  remesher->SetMaximumNumberOfIterations(0);
  const double initialVariation = EdgeLengthCoefficientOfVariation(Remesh(remesher));
  remesher->SetMaximumNumberOfIterations(100);
  const double iteratedVariation = EdgeLengthCoefficientOfVariation(Remesh(remesher));
  if (!(iteratedVariation < initialVariation))
  {
    std::cerr << "MaximumNumberOfIterations: edge length variation must decrease with iterations, got " << initialVariation << " without and "
              << iteratedVariation << " with iterations" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// Fast clustering does not make cluster sizes uniform, so it ignores the
// settings of that step and gives less uniform triangles
bool TestFastClustering()
{
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputData(Sphere(100));
  remesher->SetNumberOfClusters(500);
  vtkSmartPointer<vtkPolyData> uniform = Remesh(remesher);
  remesher->FastClusteringOn();
  vtkSmartPointer<vtkPolyData> fast = Remesh(remesher);
  remesher->SetMaximumNumberOfIterations(0);
  remesher->SetIsolatedClusterRemovalAttempts(0);
  vtkSmartPointer<vtkPolyData> fastWithoutIterations = Remesh(remesher);

  if (!CheckTriangles(fast, "FastClustering", true))
  {
    return false;
  }
  if (!SameMesh(fast, fastWithoutIterations))
  {
    std::cerr << "FastClustering: MaximumNumberOfIterations and IsolatedClusterRemovalAttempts must have no effect" << std::endl;
    return false;
  }
  const double fastVariation = EdgeLengthCoefficientOfVariation(fast);
  const double uniformVariation = EdgeLengthCoefficientOfVariation(uniform);
  if (!(fastVariation > uniformVariation))
  {
    std::cerr << "FastClustering: edge lengths must vary more than with uniform clustering, got " << fastVariation << " and " << uniformVariation
              << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// On a capped cylinder some clusters are split by the energy minimization, and
// the removal of the isolated parts changes the result
bool TestIsolatedClusterRemovalAttempts()
{
  vtkNew<vtkCylinderSource> cylinder;
  cylinder->SetResolution(30);
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputConnection(cylinder->GetOutputPort());
  remesher->SetNumberOfSubdivisions(2);
  remesher->SetNumberOfClusters(200);
  remesher->SetIsolatedClusterRemovalAttempts(0);
  vtkSmartPointer<vtkPolyData> withoutAttempts = Remesh(remesher);
  remesher->SetIsolatedClusterRemovalAttempts(10);
  vtkSmartPointer<vtkPolyData> withAttempts = Remesh(remesher);
  if (!CheckTriangles(withoutAttempts, "IsolatedClusterRemovalAttempts", false) || !CheckTriangles(withAttempts, "IsolatedClusterRemovalAttempts", false))
  {
    return false;
  }
  if (SameMesh(withoutAttempts, withAttempts))
  {
    std::cerr << "IsolatedClusterRemovalAttempts: the number of attempts must change the output" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// Projected points lie on the input surface. Otherwise they are cluster
// centroids, which are inside the convex input.
bool TestProjectPointsToSurface()
{
  vtkSmartPointer<vtkPolyData> sphere = Sphere(100);
  const double radius = 0.5;
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputData(sphere);
  remesher->SetNumberOfClusters(1000);

  double distanceRange[2];
  DistanceRangeToSurface(Remesh(remesher), sphere, distanceRange);
  if (distanceRange[1] > 1e-6)
  {
    std::cerr << "ProjectPointsToSurface: points must be on the input surface, found one at distance " << distanceRange[1] << std::endl;
    return false;
  }

  remesher->ProjectPointsToSurfaceOff();
  vtkSmartPointer<vtkPolyData> centroids = Remesh(remesher);
  DistanceRangeToSurface(centroids, sphere, distanceRange);
  if (distanceRange[1] < 1e-4)
  {
    std::cerr << "ProjectPointsToSurfaceOff: points must not be moved onto the input surface" << std::endl;
    return false;
  }
  for (vtkIdType i = 0; i < centroids->GetNumberOfPoints(); ++i)
  {
    double point[3];
    centroids->GetPoint(i, point);
    if (vtkMath::Norm(point) >= radius)
    {
      std::cerr << "ProjectPointsToSurfaceOff: point " << i << " is not inside the sphere" << std::endl;
      return false;
    }
  }
  return CheckTriangles(centroids, "ProjectPointsToSurfaceOff", true);
}

//----------------------------------------------------------------------------
// Triangles follow the orientation of the input surface, whichever it is
bool TestAlignNormals()
{
  vtkSmartPointer<vtkPolyData> sphere = Sphere(100);
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputData(sphere);
  remesher->SetNumberOfClusters(2000);
  if (!CheckTriangles(Remesh(remesher), "AlignNormals", true))
  {
    return false;
  }

  vtkNew<vtkReverseSense> reverse;
  reverse->SetInputData(sphere);
  reverse->ReverseCellsOn();
  reverse->Update();
  remesher->SetInputData(reverse->GetOutput());
  vtkIdType outward = 0;
  vtkIdType inward = 0;
  CountTriangleOrientations(Remesh(remesher), outward, inward);
  if (outward != 0 || inward == 0)
  {
    std::cerr << "AlignNormals: with a reversed input all triangles must face inward, " << outward << " face outward" << std::endl;
    return false;
  }

  // Without alignment the orientation follows the order of the cluster ids
  remesher->SetInputData(sphere);
  remesher->AlignNormalsOff();
  CountTriangleOrientations(Remesh(remesher), outward, inward);
  if (outward == 0 || inward == 0)
  {
    std::cerr << "AlignNormalsOff: expected triangles in both orientations, got " << outward << " outward and " << inward << " inward" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// On this arrow one cluster is part of no output triangle. Cleaning removes
// its point, otherwise there is one point per cluster.
bool TestCleanOutput()
{
  vtkNew<vtkArrowSource> arrow;
  arrow->SetTipResolution(30);
  arrow->SetShaftResolution(30);
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputConnection(arrow->GetOutputPort());
  remesher->SetNumberOfSubdivisions(2);
  remesher->SetNumberOfClusters(200);
  remesher->CleanOutputOff();
  vtkSmartPointer<vtkPolyData> raw = Remesh(remesher);
  remesher->CleanOutputOn();
  vtkSmartPointer<vtkPolyData> cleaned = Remesh(remesher);

  if (!CheckNumberOfPoints(raw, "CleanOutputOff", 200))
  {
    return false;
  }
  const vtkIdType numberOfUnusedPoints = NumberOfUnusedPoints(raw);
  if (numberOfUnusedPoints == 0)
  {
    std::cerr << "CleanOutputOff: expected points that are not used by any triangle" << std::endl;
    return false;
  }
  if (!CheckNumberOfPoints(cleaned, "CleanOutputOn", 200 - numberOfUnusedPoints) || NumberOfUnusedPoints(cleaned) != 0)
  {
    std::cerr << "CleanOutputOn: unused points must be removed" << std::endl;
    return false;
  }
  if (cleaned->GetNumberOfPolys() != raw->GetNumberOfPolys())
  {
    std::cerr << "CleanOutputOn: cleaning must not change the triangles" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
// Strips are triangulated, vertex and line cells are ignored
bool TestInputCellTypes()
{
  vtkSmartPointer<vtkPolyData> sphere = Sphere(40);
  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetNumberOfClusters(500);
  remesher->SetInputData(sphere);
  vtkSmartPointer<vtkPolyData> fromTriangles = Remesh(remesher);

  vtkNew<vtkStripper> stripper;
  stripper->SetInputData(sphere);
  stripper->Update();
  if (stripper->GetOutput()->GetNumberOfStrips() == 0)
  {
    std::cerr << "Strips: the test input must contain strips" << std::endl;
    return false;
  }
  remesher->SetInputConnection(stripper->GetOutputPort());
  vtkSmartPointer<vtkPolyData> fromStrips = Remesh(remesher);
  if (!CheckNumberOfPoints(fromStrips, "Strips", 500) || !CheckTriangles(fromStrips, "Strips", true))
  {
    return false;
  }

  vtkNew<vtkPolyData> verticesAndLines;
  verticesAndLines->DeepCopy(sphere);
  vtkNew<vtkCellArray> vertices;
  vertices->InsertNextCell({ 0 });
  vtkNew<vtkCellArray> lines;
  lines->InsertNextCell({ 0, 5 });
  verticesAndLines->SetVerts(vertices);
  verticesAndLines->SetLines(lines);
  remesher->SetInputData(verticesAndLines);
  if (!SameMesh(Remesh(remesher), fromTriangles))
  {
    std::cerr << "Vertices and lines: they must be ignored" << std::endl;
    return false;
  }

  // Meshes with one or two triangles have too few faces to project onto
  vtkNew<vtkPoints> points;
  points->InsertNextPoint(0, 0, 0);
  points->InsertNextPoint(1, 0, 0);
  points->InsertNextPoint(0, 1, 0);
  points->InsertNextPoint(1, 1, 0);
  vtkNew<vtkCellArray> triangles;
  triangles->InsertNextCell({ 0, 1, 2 });
  vtkNew<vtkPolyData> oneTriangle;
  oneTriangle->SetPoints(points);
  oneTriangle->SetPolys(triangles);
  remesher->SetInputData(oneTriangle);
  remesher->SetNumberOfClusters(10);
  if (!CheckTriangles(Remesh(remesher), "One triangle", false))
  {
    return false;
  }
  vtkNew<vtkCellArray> twoTriangles;
  twoTriangles->InsertNextCell({ 0, 1, 2 });
  twoTriangles->InsertNextCell({ 1, 3, 2 });
  vtkNew<vtkPolyData> square;
  square->SetPoints(points);
  square->SetPolys(twoTriangles);
  remesher->SetInputData(square);
  remesher->SetNumberOfSubdivisions(2);
  if (!CheckTriangles(Remesh(remesher), "Two triangles", false))
  {
    return false;
  }
  return true;
}

} // namespace

//----------------------------------------------------------------------------
int vtkACVDRemeshFilterTest(int vtkNotUsed(argc), char* vtkNotUsed(argv)[])
{
  vtkNew<vtkSphereSource> sphere;
  sphere->SetThetaResolution(100);
  sphere->SetPhiResolution(100);
  sphere->Update();
  const vtkIdType numberOfSpherePoints = sphere->GetOutput()->GetNumberOfPoints();

  vtkNew<vtkACVDRemeshFilter> remesher;
  remesher->SetInputConnection(sphere->GetOutputPort());

  // Default clustering gives one output point per cluster
  remesher->SetNumberOfClusters(2000);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "Default", 2000) || !CheckTriangles(remesher->GetOutput(), "Default", true))
  {
    return EXIT_FAILURE;
  }

  // Subdividing allows more clusters than input points
  remesher->SetNumberOfSubdivisions(1);
  remesher->SetNumberOfClusters(20000);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "Subdivided", 20000) || !CheckTriangles(remesher->GetOutput(), "Subdivided", true))
  {
    return EXIT_FAILURE;
  }

  // With more clusters than points every point is a cluster
  remesher->SetNumberOfSubdivisions(0);
  remesher->SetNumberOfClusters(100000);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "More clusters than points", numberOfSpherePoints)
      || !CheckTriangles(remesher->GetOutput(), "More clusters than points", true))
  {
    return EXIT_FAILURE;
  }

  // Fast clustering
  remesher->SetNumberOfClusters(2000);
  remesher->FastClusteringOn();
  remesher->Update();
  if (remesher->GetOutput()->GetNumberOfPoints() == 0)
  {
    std::cerr << "Fast: output must not be empty" << std::endl;
    return EXIT_FAILURE;
  }
  if (!CheckTriangles(remesher->GetOutput(), "Fast", false))
  {
    return EXIT_FAILURE;
  }
  remesher->FastClusteringOff();

  // Empty input gives empty output
  vtkNew<vtkPolyData> emptyPolyData;
  remesher->SetInputData(emptyPolyData);
  remesher->Update();
  if (!CheckNumberOfPoints(remesher->GetOutput(), "Empty", 0))
  {
    return EXIT_FAILURE;
  }

  if (!TestClamping() || !TestMaximumNumberOfIterations() || !TestFastClustering() || !TestIsolatedClusterRemovalAttempts()
      || !TestProjectPointsToSurface() || !TestAlignNormals() || !TestCleanOutput() || !TestInputCellTypes())
  {
    return EXIT_FAILURE;
  }

  std::cout << "vtkACVDRemeshFilterTest passed" << std::endl;
  return EXIT_SUCCESS;
}
