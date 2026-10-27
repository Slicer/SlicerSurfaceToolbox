/*==============================================================================

  Program: 3D Slicer

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file is a C++ port of pyacvd (https://github.com/pyvista/pyacvd):

  MIT License

  Copyright (c) 2017-2024 The PyVista Developers

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

==============================================================================*/

#include "vtkACVDRemeshFilter.h"

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellArrayIterator.h>
#include <vtkCleanPolyData.h>
#include <vtkDoubleArray.h>
#include <vtkIdList.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStaticPointLocator.h>
#include <vtkTriangleFilter.h>

// STD includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <numeric>
#include <utility>
#include <vector>

vtkStandardNewMacro(vtkACVDRemeshFilter);

// The functions below follow pyacvd's clustering.cpp and clustering.py
// operation by operation, so that the output matches pyacvd exactly.
// The order of neighbors and edges drives how clusters grow and in which
// order points are moved between them, so it must not be changed.
namespace
{
constexpr double VAL_1_3 = 0.3333333333333333333;
constexpr double EPSILON = 1E-6;
constexpr int TRI_EDGES[3][2] = { { 0, 1 }, { 1, 2 }, { 2, 0 } };

// Number of face centroids that are tried first when projecting a cluster
// onto the surface. pyacvd always fetches 1000, but almost every ray hits
// one of the closest few faces, so the full list is only fetched on a miss.
constexpr vtkIdType INITIAL_PROJECTION_CANDIDATES = 16;
constexpr vtkIdType MAXIMUM_PROJECTION_CANDIDATES = 1000;

//----------------------------------------------------------------------------
// Cross product of the two edges of a triangle starting at v0
void TriangleCross(const double* v0, const double* v1, const double* v2, double c[3])
{
  const double e0[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
  const double e1[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
  c[0] = e0[1] * e1[2] - e0[2] * e1[1];
  c[1] = e0[2] * e1[0] - e0[0] * e1[2];
  c[2] = e0[0] * e1[1] - e0[1] * e1[0];
}

//----------------------------------------------------------------------------
double Length(const double c[3])
{
  return std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
}

//----------------------------------------------------------------------------
// Unit normal of a triangle, or its unnormalized cross product if it has no area
void FaceNormal(const double* v0, const double* v1, const double* v2, double normal[3])
{
  TriangleCross(v0, v1, v2, normal);
  const double length = Length(normal);
  if (length > 0)
  {
    const double lengthInv = 1 / length;
    normal[0] *= lengthInv;
    normal[1] *= lengthInv;
    normal[2] *= lengthInv;
  }
}

//----------------------------------------------------------------------------
// Split each triangle that has a nonzero area into four. The three new
// midpoints are added for each split triangle, without sharing them with the
// neighbor triangles, so the result must be cleaned.
void SubdivideTriangles(std::vector<double>& points, std::vector<vtkIdType>& faces)
{
  const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);

  std::vector<double> newPoints;
  newPoints.reserve(points.size() + faces.size() * 3);
  newPoints = points;
  std::vector<vtkIdType> newFaces;
  newFaces.reserve(faces.size() * 4);

  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    const vtkIdType p0 = faces[i * 3 + 0];
    const vtkIdType p1 = faces[i * 3 + 1];
    const vtkIdType p2 = faces[i * 3 + 2];
    const double* v0 = &points[p0 * 3];
    const double* v1 = &points[p1 * 3];
    const double* v2 = &points[p2 * 3];

    double c[3];
    TriangleCross(v0, v1, v2, c);
    if (0.5 * Length(c) > 0.0)
    {
      const vtkIdType n = static_cast<vtkIdType>(newPoints.size() / 3);
      newFaces.insert(newFaces.end(), { p0, n, n + 2, p1, n + 1, n, p2, n + 2, n + 1, n, n + 1, n + 2 });
      for (int k = 0; k < 3; ++k)
      {
        newPoints.push_back((v0[k] + v1[k]) * 0.5);
      }
      for (int k = 0; k < 3; ++k)
      {
        newPoints.push_back((v1[k] + v2[k]) * 0.5);
      }
      for (int k = 0; k < 3; ++k)
      {
        newPoints.push_back((v0[k] + v2[k]) * 0.5);
      }
    }
    else
    {
      newFaces.insert(newFaces.end(), { p0, p1, p2 });
    }
  }

  points.swap(newPoints);
  faces.swap(newFaces);
}

//----------------------------------------------------------------------------
// Point weight is the sum of the areas of the faces that use the point.
// Weighted points are the points multiplied by their weight.
void PointWeights(const std::vector<double>& points,
                  const std::vector<vtkIdType>& faces,
                  std::vector<double>& area,
                  std::vector<double>& weightedPoints)
{
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(points.size() / 3);
  const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);

  area.assign(numberOfPoints, 0.0);
  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    const vtkIdType p0 = faces[i * 3 + 0];
    const vtkIdType p1 = faces[i * 3 + 1];
    const vtkIdType p2 = faces[i * 3 + 2];
    double c[3];
    TriangleCross(&points[p0 * 3], &points[p1 * 3], &points[p2 * 3], c);
    const double faceArea = 0.5 * Length(c);
    area[p0] += faceArea;
    area[p1] += faceArea;
    area[p2] += faceArea;
  }

  weightedPoints.resize(numberOfPoints * 3);
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    for (int k = 0; k < 3; ++k)
    {
      weightedPoints[i * 3 + k] = area[i] * points[i * 3 + k];
    }
  }
}

//----------------------------------------------------------------------------
// Neighbor points of each point, in compressed row storage: the neighbors of
// point i are neighbors[offsets[i]] ... neighbors[offsets[i + 1] - 1].
void NeighborsFromTriFaces(vtkIdType numberOfPoints,
                           const std::vector<vtkIdType>& faces,
                           std::vector<vtkIdType>& neighbors,
                           std::vector<vtkIdType>& offsets)
{
  const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);

  // Upper bound of the number of neighbors of each point
  std::vector<vtkIdType> numberOfNeighbors(numberOfPoints, 0);
  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      numberOfNeighbors[faces[i * 3 + TRI_EDGES[j][0]]] += 1;
      numberOfNeighbors[faces[i * 3 + TRI_EDGES[j][1]]] += 1;
    }
  }

  std::vector<vtkIdType> lookup(numberOfPoints + 1);
  vtkIdType cumulativeSum = 0;
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    lookup[i] = cumulativeSum;
    cumulativeSum += numberOfNeighbors[i];
  }
  lookup[numberOfPoints] = cumulativeSum;

  // Add each neighbor once
  std::fill(numberOfNeighbors.begin(), numberOfNeighbors.end(), 0);
  std::vector<vtkIdType> adjacency(cumulativeSum, -1);
  auto addNeighbor = [&](vtkIdType pointId, vtkIdType neighborId)
  {
    const vtkIdType* first = adjacency.data() + lookup[pointId];
    const vtkIdType* last = first + numberOfNeighbors[pointId];
    if (std::find(first, last, neighborId) == last)
    {
      adjacency[lookup[pointId] + numberOfNeighbors[pointId]] = neighborId;
      numberOfNeighbors[pointId] += 1;
    }
  };
  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      const vtkIdType edgeA = faces[i * 3 + TRI_EDGES[j][0]];
      const vtkIdType edgeB = faces[i * 3 + TRI_EDGES[j][1]];
      addNeighbor(edgeA, edgeB);
      addNeighbor(edgeB, edgeA);
    }
  }

  // Compress now that the true number of neighbors is known
  neighbors.resize(cumulativeSum);
  cumulativeSum = 0;
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    const vtkIdType index = lookup[i];
    std::copy(adjacency.begin() + index, adjacency.begin() + index + numberOfNeighbors[i], neighbors.begin() + cumulativeSum);
    lookup[i] = cumulativeSum;
    cumulativeSum += numberOfNeighbors[i];
  }
  lookup[numberOfPoints] = cumulativeSum;
  neighbors.resize(cumulativeSum);
  offsets.swap(lookup);
}

//----------------------------------------------------------------------------
// Each edge once, as consecutive point id pairs
void UniqueEdges(const std::vector<vtkIdType>& neighbors, const std::vector<vtkIdType>& offsets, std::vector<vtkIdType>& edges)
{
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(offsets.size()) - 1;
  std::vector<char> visited(neighbors.size(), 0);

  edges.clear();
  edges.reserve(neighbors.size());
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    for (vtkIdType j = offsets[i]; j < offsets[i + 1]; ++j)
    {
      if (visited[j])
      {
        continue;
      }
      const vtkIdType neighborId = neighbors[j];
      edges.push_back(i);
      edges.push_back(neighborId);
      visited[j] = 1;
      // Mark the reciprocal edge as visited
      for (vtkIdType k = offsets[neighborId]; k < offsets[neighborId + 1]; ++k)
      {
        if (neighbors[k] == i)
        {
          visited[k] = 1;
          break;
        }
      }
    }
  }
}

//----------------------------------------------------------------------------
// Grow each cluster ring by ring from the first unassigned point until it
// reaches its share of the remaining area.
void InitializeClusters(std::vector<vtkIdType>& clusters,
                        const std::vector<vtkIdType>& neighbors,
                        const std::vector<vtkIdType>& offsets,
                        const std::vector<double>& area,
                        vtkIdType numberOfClusters)
{
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(area.size());

  double areaRemaining = 0;
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    areaRemaining += area[i];
  }

  // Starting point to search for the first unassigned point
  vtkIdType lastIndex = 0;

  std::vector<vtkIdType> currentRing;
  std::vector<vtkIdType> nextRing;

  const double clusterTargetArea = areaRemaining / numberOfClusters;
  for (vtkIdType i = 0; i < numberOfClusters; ++i)
  {
    const double targetArea = areaRemaining - clusterTargetArea * (numberOfClusters - i - 1);
    double clusterArea = 0.0;

    while (lastIndex < numberOfPoints && clusters[lastIndex] != -1)
    {
      lastIndex++;
    }
    if (lastIndex >= numberOfPoints)
    {
      break; // no unassigned points left
    }

    clusterArea += area[lastIndex];
    currentRing.push_back(lastIndex);
    clusters[lastIndex] = i;

    while (!currentRing.empty())
    {
      for (vtkIdType pointId : currentRing)
      {
        for (vtkIdType k = offsets[pointId]; k < offsets[pointId + 1]; ++k)
        {
          const vtkIdType neighborId = neighbors[k];
          if (clusters[neighborId] == -1 && area[neighborId] + clusterArea < targetArea)
          {
            clusterArea += area[neighborId];
            clusters[neighborId] = i;
            nextRing.push_back(neighborId);
          }
        }
      }
      currentRing.swap(nextRing);
      nextRing.clear();
    }

    areaRemaining -= clusterArea;
  }
}

//----------------------------------------------------------------------------
// Assign unassigned points to the cluster of a neighbor along the edges.
// Returns true if some edges could not be resolved.
bool GrowNull(const std::vector<vtkIdType>& edges, std::vector<vtkIdType>& clusters)
{
  const vtkIdType numberOfEdges = static_cast<vtkIdType>(edges.size() / 2);

  // Edges where both points are unassigned
  std::vector<vtkIdType> unassignedEdges;
  for (vtkIdType i = 0; i < numberOfEdges; ++i)
  {
    const vtkIdType pointA = edges[2 * i];
    const vtkIdType pointB = edges[2 * i + 1];
    const vtkIdType clusterA = clusters[pointA];
    const vtkIdType clusterB = clusters[pointB];
    if (clusterA == -1 && clusterB == -1)
    {
      unassignedEdges.push_back(i);
    }
    else
    {
      if (clusterA == -1)
      {
        clusters[pointA] = clusterB;
      }
      if (clusterB == -1)
      {
        clusters[pointB] = clusterA;
      }
    }
  }

  // As in pyacvd, edges that end up with both points assigned are also kept,
  // which only affects the returned value.
  bool changed = true;
  while (changed && !unassignedEdges.empty())
  {
    changed = false;
    size_t numberOfKeptEdges = 0;
    for (vtkIdType i : unassignedEdges)
    {
      const vtkIdType pointA = edges[2 * i];
      const vtkIdType pointB = edges[2 * i + 1];
      const vtkIdType clusterA = clusters[pointA];
      const vtkIdType clusterB = clusters[pointB];
      if (clusterA == -1 && clusterB != -1)
      {
        clusters[pointA] = clusterB;
        changed = true;
      }
      else if (clusterB == -1 && clusterA != -1)
      {
        clusters[pointB] = clusterA;
        changed = true;
      }
      else
      {
        unassignedEdges[numberOfKeptEdges++] = i;
      }
    }
    unassignedEdges.resize(numberOfKeptEdges);
  }

  return !unassignedEdges.empty();
}

//----------------------------------------------------------------------------
// Reassign points that were not reached from their cluster's first point
// to the cluster of a neighbor. Returns the number of such reassignments.
vtkIdType GrowUnvisited(const std::vector<vtkIdType>& edges, std::vector<vtkIdType>& clusters, std::vector<char>& visited)
{
  const vtkIdType numberOfEdges = static_cast<vtkIdType>(edges.size() / 2);

  std::vector<vtkIdType> unvisitedEdges;
  vtkIdType numberOfIsolated = 0;
  for (vtkIdType i = 0; i < numberOfEdges; ++i)
  {
    const vtkIdType pointA = edges[2 * i];
    const vtkIdType pointB = edges[2 * i + 1];
    const vtkIdType clusterA = clusters[pointA];
    const vtkIdType clusterB = clusters[pointB];
    if (!visited[pointA] && !visited[pointB])
    {
      unvisitedEdges.push_back(i);
      numberOfIsolated++;
    }
    else
    {
      if (!visited[pointA] && visited[pointB])
      {
        clusters[pointA] = clusterB;
        visited[pointA] = 1;
        numberOfIsolated++;
      }
      if (!visited[pointB] && visited[pointA])
      {
        clusters[pointB] = clusterA;
        visited[pointB] = 1;
        numberOfIsolated++;
      }
    }
  }

  bool changed = true;
  while (changed && !unvisitedEdges.empty())
  {
    changed = false;
    size_t numberOfKeptEdges = 0;
    for (vtkIdType i : unvisitedEdges)
    {
      const vtkIdType pointA = edges[2 * i];
      const vtkIdType pointB = edges[2 * i + 1];
      const vtkIdType clusterA = clusters[pointA];
      const vtkIdType clusterB = clusters[pointB];
      // pyacvd tests `~visited[a] && visited[b]` and `visited[a] && ~visited[b]`.
      // `~` of a bool is never zero, so only the other operand matters.
      // That behavior is kept so that the output stays the same.
      if (visited[pointB])
      {
        clusters[pointA] = clusterB;
        changed = true;
      }
      else if (visited[pointA])
      {
        clusters[pointB] = clusterA;
        changed = true;
      }
      else
      {
        unvisitedEdges[numberOfKeptEdges++] = i;
      }
    }
    unvisitedEdges.resize(numberOfKeptEdges);
  }

  return numberOfIsolated;
}

//----------------------------------------------------------------------------
// Energy of a cluster after adding a point
double EnergyWith(const double* sgamma, const double* centroid, double srho, double pointArea)
{
  return ((sgamma[0] + centroid[0]) * (sgamma[0] + centroid[0]) + (sgamma[1] + centroid[1]) * (sgamma[1] + centroid[1])
          + (sgamma[2] + centroid[2]) * (sgamma[2] + centroid[2]))
         / (srho + pointArea);
}

//----------------------------------------------------------------------------
// Energy of a cluster after removing a point
double EnergyWithout(const double* sgamma, const double* centroid, double srho, double pointArea)
{
  return ((sgamma[0] - centroid[0]) * (sgamma[0] - centroid[0]) + (sgamma[1] - centroid[1]) * (sgamma[1] - centroid[1])
          + (sgamma[2] - centroid[2]) * (sgamma[2] - centroid[2]))
         / (srho - pointArea);
}

//----------------------------------------------------------------------------
// Move points between neighbor clusters while that increases the total
// energy (sum of |sgamma|^2 / srho), which makes cluster sizes uniform.
// Returns the number of iterations.
int MinimizeEnergy(const std::vector<vtkIdType>& edges,
                   std::vector<vtkIdType>& clusters,
                   const std::vector<double>& area,
                   const std::vector<double>& centroids,
                   std::vector<double>& sgamma,
                   std::vector<double>& srho,
                   std::vector<double>& energy,
                   vtkIdType numberOfClusters,
                   int maximumNumberOfIterations)
{
  const vtkIdType numberOfEdges = static_cast<vtkIdType>(edges.size() / 2);

  std::vector<vtkIdType> clusterCount(numberOfClusters, 0);
  for (vtkIdType clusterId : clusters)
  {
    clusterCount[clusterId]++;
  }

  // Clusters modified in the current and the next iteration
  std::vector<char> modifiedCurrent(numberOfClusters, 0);
  std::vector<char> modifiedNext(numberOfClusters, 1);

  auto movePoint = [&](vtkIdType pointId, vtkIdType fromCluster, vtkIdType toCluster, double fromEnergy, double toEnergy)
  {
    modifiedNext[fromCluster] = 1;
    modifiedNext[toCluster] = 1;

    clusters[pointId] = toCluster;
    clusterCount[fromCluster]--;
    clusterCount[toCluster]++;

    const double* pointCentroid = &centroids[pointId * 3];
    srho[toCluster] += area[pointId];
    for (int k = 0; k < 3; ++k)
    {
      sgamma[toCluster * 3 + k] += pointCentroid[k];
    }
    srho[fromCluster] -= area[pointId];
    for (int k = 0; k < 3; ++k)
    {
      sgamma[fromCluster * 3 + k] -= pointCentroid[k];
    }

    energy[fromCluster] = fromEnergy;
    energy[toCluster] = toEnergy;
  };

  bool changed = true;
  int iteration = 0;
  while (changed && iteration < maximumNumberOfIterations)
  {
    std::swap(modifiedCurrent, modifiedNext);
    std::fill(modifiedNext.begin(), modifiedNext.end(), 0);

    changed = false;
    for (vtkIdType i = 0; i < numberOfEdges; ++i)
    {
      const vtkIdType pointA = edges[2 * i];
      const vtkIdType pointB = edges[2 * i + 1];
      const vtkIdType clusterA = clusters[pointA];
      const vtkIdType clusterB = clusters[pointB];

      // Skip if the edge does not cross two clusters or if both are unmodified
      if (clusterA == clusterB || (!modifiedCurrent[clusterA] && !modifiedCurrent[clusterB]))
      {
        continue;
      }

      const double* sgammaA = &sgamma[clusterA * 3];
      const double* sgammaB = &sgamma[clusterB * 3];
      const double originalEnergy = energy[clusterA] + energy[clusterB];

      if (clusterCount[clusterA] > 1 && clusterCount[clusterB] > 1)
      {
        // Energy with point b moved to cluster a
        const double energyAWithB = EnergyWith(sgammaA, &centroids[pointB * 3], srho[clusterA], area[pointB]);
        const double energyBWithoutB = EnergyWithout(sgammaB, &centroids[pointB * 3], srho[clusterB], area[pointB]);
        const double energyMoveB = energyAWithB + energyBWithoutB;

        // Energy with point a moved to cluster b
        const double energyAWithoutA = EnergyWithout(sgammaA, &centroids[pointA * 3], srho[clusterA], area[pointA]);
        const double energyBWithA = EnergyWith(sgammaB, &centroids[pointA * 3], srho[clusterB], area[pointA]);
        const double energyMoveA = energyAWithoutA + energyBWithA;

        if (energyMoveB > originalEnergy && energyMoveB > energyMoveA)
        {
          changed = true;
          movePoint(pointB, clusterB, clusterA, energyBWithoutB, energyAWithB);
        }
        else if (energyMoveA > originalEnergy && energyMoveA > energyMoveB)
        {
          changed = true;
          movePoint(pointA, clusterA, clusterB, energyAWithoutA, energyBWithA);
        }
      }
      else if (clusterCount[clusterA] > 1)
      {
        const double energyAWithoutA = EnergyWithout(sgammaA, &centroids[pointA * 3], srho[clusterA], area[pointA]);
        const double energyBWithA = EnergyWith(sgammaB, &centroids[pointA * 3], srho[clusterB], area[pointA]);
        if (energyAWithoutA + energyBWithA > originalEnergy)
        {
          changed = true;
          movePoint(pointA, clusterA, clusterB, energyAWithoutA, energyBWithA);
        }
      }
      else if (clusterCount[clusterB] > 1)
      {
        const double energyAWithB = EnergyWith(sgammaA, &centroids[pointB * 3], srho[clusterA], area[pointB]);
        const double energyBWithoutB = EnergyWithout(sgammaB, &centroids[pointB * 3], srho[clusterB], area[pointB]);
        if (energyAWithB + energyBWithoutB > originalEnergy)
        {
          changed = true;
          movePoint(pointB, clusterB, clusterA, energyBWithoutB, energyAWithB);
        }
      }
    }

    iteration++;
  }

  return iteration;
}

//----------------------------------------------------------------------------
// Find the parts of clusters that are not connected to the first point of
// the cluster and give them to a neighbor cluster. Returns the number of
// reassignments.
vtkIdType GrowIsolatedClusters(vtkIdType numberOfClusters,
                               const std::vector<vtkIdType>& neighbors,
                               const std::vector<vtkIdType>& offsets,
                               std::vector<vtkIdType>& clusters,
                               const std::vector<vtkIdType>& edges)
{
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(clusters.size());
  std::vector<char> visited(numberOfPoints, 0);
  std::vector<char> visitedCluster(numberOfClusters, 0);

  std::vector<vtkIdType> currentFront;
  std::vector<vtkIdType> nextFront;

  vtkIdType numberOfClustersChecked = 0;
  vtkIdType foundIndex = 0;
  while (numberOfClustersChecked < numberOfClusters)
  {
    // First unvisited point of an unvisited cluster
    bool foundUnvisited = false;
    for (vtkIdType i = foundIndex; i < numberOfPoints; ++i)
    {
      if (!visited[i] && !visitedCluster[clusters[i]])
      {
        foundIndex = i;
        numberOfClustersChecked++;
        foundUnvisited = true;
        break;
      }
    }
    if (!foundUnvisited)
    {
      break;
    }

    const vtkIdType currentCluster = clusters[foundIndex];
    visited[foundIndex] = 1;
    visitedCluster[currentCluster] = 1;

    // Visit every point of the cluster connected to the first one
    nextFront.push_back(foundIndex);
    foundIndex++;
    do
    {
      std::swap(currentFront, nextFront);
      nextFront.clear();
      for (vtkIdType pointId : currentFront)
      {
        for (vtkIdType k = offsets[pointId]; k < offsets[pointId + 1]; ++k)
        {
          const vtkIdType neighborId = neighbors[k];
          if (clusters[neighborId] == currentCluster && !visited[neighborId])
          {
            nextFront.push_back(neighborId);
            visited[neighborId] = 1;
          }
        }
      }
    } while (!nextFront.empty());
  }

  return GrowUnvisited(edges, clusters, visited);
}

//----------------------------------------------------------------------------
// Number the clusters consecutively in the order of their first point.
// Returns the number of clusters.
vtkIdType RenumberClusters(std::vector<vtkIdType>& clusters, vtkIdType numberOfClusters)
{
  std::vector<char> assigned(numberOfClusters, 0);
  std::vector<vtkIdType> newIds(numberOfClusters);
  vtkIdType count = 0;
  for (vtkIdType& clusterId : clusters)
  {
    if (!assigned[clusterId])
    {
      assigned[clusterId] = 1;
      newIds[clusterId] = count++;
    }
    clusterId = newIds[clusterId];
  }
  return count;
}

//----------------------------------------------------------------------------
void InitializeAndGrowNull(std::vector<vtkIdType>& clusters,
                           const std::vector<vtkIdType>& neighbors,
                           const std::vector<vtkIdType>& offsets,
                           const std::vector<double>& area,
                           const std::vector<vtkIdType>& edges,
                           vtkIdType numberOfClusters)
{
  clusters.assign(area.size(), -1);
  InitializeClusters(clusters, neighbors, offsets, area, numberOfClusters);
  if (GrowNull(edges, clusters))
  {
    std::replace(clusters.begin(), clusters.end(), vtkIdType(-1), vtkIdType(0));
  }
}

//----------------------------------------------------------------------------
// Clusters without making their sizes uniform. Returns the number of clusters.
vtkIdType FastCluster(std::vector<vtkIdType>& clusters,
                      const std::vector<vtkIdType>& neighbors,
                      const std::vector<vtkIdType>& offsets,
                      const std::vector<double>& area,
                      const std::vector<vtkIdType>& edges,
                      vtkIdType numberOfClusters)
{
  InitializeAndGrowNull(clusters, neighbors, offsets, area, edges, numberOfClusters);
  return RenumberClusters(clusters, numberOfClusters);
}

//----------------------------------------------------------------------------
// Clusters of uniform size. Returns the number of clusters.
vtkIdType Cluster(std::vector<vtkIdType>& clusters,
                  const std::vector<vtkIdType>& neighbors,
                  const std::vector<vtkIdType>& offsets,
                  const std::vector<double>& area,
                  const std::vector<double>& centroids,
                  const std::vector<vtkIdType>& edges,
                  vtkIdType numberOfClusters,
                  int maximumNumberOfIterations,
                  int isolatedClusterRemovalAttempts)
{
  InitializeAndGrowNull(clusters, neighbors, offsets, area, edges, numberOfClusters);
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(clusters.size());

  // Cluster mass and weighted centroid sum
  std::vector<double> sgamma(numberOfClusters * 3, 0.0);
  std::vector<double> srho(numberOfClusters, 0.0);
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    const vtkIdType clusterId = clusters[i];
    srho[clusterId] += area[i];
    for (int k = 0; k < 3; ++k)
    {
      sgamma[clusterId * 3 + k] += centroids[i * 3 + k];
    }
  }

  std::vector<double> energy(numberOfClusters);
  for (vtkIdType i = 0; i < numberOfClusters; ++i)
  {
    const double* s = &sgamma[i * 3];
    energy[i] = (s[0] * s[0] + s[1] * s[1] + s[2] * s[2]) / srho[i];
  }

  MinimizeEnergy(edges, clusters, area, centroids, sgamma, srho, energy, numberOfClusters, maximumNumberOfIterations);

  // Remove isolated parts of clusters. As in pyacvd, cluster masses and
  // centroids are not updated for the reassigned points.
  vtkIdType numberOfDisconnected = GrowIsolatedClusters(numberOfClusters, neighbors, offsets, clusters, edges);
  int attempt = 0;
  while (numberOfDisconnected && attempt < isolatedClusterRemovalAttempts)
  {
    MinimizeEnergy(edges, clusters, area, centroids, sgamma, srho, energy, numberOfClusters, maximumNumberOfIterations);
    numberOfDisconnected = GrowIsolatedClusters(numberOfClusters, neighbors, offsets, clusters, edges);
    attempt++;
  }

  std::replace(clusters.begin(), clusters.end(), vtkIdType(-1), vtkIdType(0));
  return RenumberClusters(clusters, numberOfClusters);
}

//----------------------------------------------------------------------------
// Normalized sum of the unit normals of the faces around each point
void PointNormals(const std::vector<double>& points, const std::vector<vtkIdType>& faces, std::vector<double>& pointNormals)
{
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(points.size() / 3);
  const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);

  pointNormals.assign(numberOfPoints * 3, 0.0);
  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    const vtkIdType* face = &faces[i * 3];
    double normal[3];
    FaceNormal(&points[face[0] * 3], &points[face[1] * 3], &points[face[2] * 3], normal);
    for (int j = 0; j < 3; ++j)
    {
      for (int k = 0; k < 3; ++k)
      {
        pointNormals[face[j] * 3 + k] += normal[k];
      }
    }
  }

  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    double* normal = &pointNormals[i * 3];
    const double length = Length(normal);
    if (length != 0)
    {
      const double lengthInv = 1 / length;
      normal[0] *= lengthInv;
      normal[1] *= lengthInv;
      normal[2] *= lengthInv;
    }
  }
}

//----------------------------------------------------------------------------
// Signed distance along the direction to the first of the candidate faces
// that the line through the source point intersects (Moller-Trumbore).
// Returns false if no candidate face is intersected.
bool RayTrace(const double* source,
              const double* direction,
              const std::vector<double>& points,
              const std::vector<vtkIdType>& faces,
              const std::vector<vtkIdType>& candidateFaces,
              double& distance)
{
  for (vtkIdType faceId : candidateFaces)
  {
    const double* v0 = &points[faces[faceId * 3 + 0] * 3];
    const double* v1 = &points[faces[faceId * 3 + 1] * 3];
    const double* v2 = &points[faces[faceId * 3 + 2] * 3];
    const double e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
    const double e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

    const double p[3] = { direction[1] * e2[2] - direction[2] * e2[1],
                          direction[2] * e2[0] - direction[0] * e2[2],
                          direction[0] * e2[1] - direction[1] * e2[0] };
    const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (std::abs(det) < EPSILON)
    {
      continue;
    }
    const double invDet = 1.0 / det;

    const double t[3] = { source[0] - v0[0], source[1] - v0[1], source[2] - v0[2] };
    const double u = (t[0] * p[0] + t[1] * p[1] + t[2] * p[2]) * invDet;
    if (u < -EPSILON || u > 1.0 + EPSILON)
    {
      continue;
    }

    const double q[3] = { t[1] * e1[2] - t[2] * e1[1], t[2] * e1[0] - t[0] * e1[2], t[0] * e1[1] - t[1] * e1[0] };
    const double v = (direction[0] * q[0] + direction[1] * q[1] + direction[2] * q[2]) * invDet;
    if (v < -EPSILON || u + v > 1.0 + EPSILON)
    {
      continue;
    }

    if (!(u + v < 1.0 + EPSILON))
    {
      continue;
    }
    const double faceDistance = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * invDet;
    if (std::abs(faceDistance) < std::numeric_limits<double>::infinity())
    {
      distance = faceDistance;
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------
// Ids of the numberOfFaces face centroids closest to x, sorted by distance.
// Points within a growing radius are collected until there are enough of them,
// which is exact and needs a single sort.
void FindClosestFaces(vtkStaticPointLocator* locator,
                      const double* faceCentroids,
                      const double x[3],
                      vtkIdType numberOfFaces,
                      double radius,
                      double maximumRadius,
                      vtkIdList* idsWithinRadius,
                      std::vector<std::pair<double, vtkIdType>>& distanceIds,
                      std::vector<vtkIdType>& closestFaces)
{
  radius = std::min(radius > 0 ? radius : maximumRadius, maximumRadius);
  for (;;)
  {
    locator->FindPointsWithinRadius(radius, x, idsWithinRadius);
    if (idsWithinRadius->GetNumberOfIds() >= numberOfFaces || radius >= maximumRadius)
    {
      break;
    }
    radius = std::min(2.0 * radius, maximumRadius);
  }

  distanceIds.clear();
  for (vtkIdType i = 0; i < idsWithinRadius->GetNumberOfIds(); ++i)
  {
    const vtkIdType faceId = idsWithinRadius->GetId(i);
    distanceIds.emplace_back(vtkMath::Distance2BetweenPoints(x, &faceCentroids[faceId * 3]), faceId);
  }
  const vtkIdType count = std::min(numberOfFaces, static_cast<vtkIdType>(distanceIds.size()));
  std::partial_sort(distanceIds.begin(), distanceIds.begin() + count, distanceIds.end());

  closestFaces.resize(count);
  for (vtkIdType i = 0; i < count; ++i)
  {
    closestFaces[i] = distanceIds[i].second;
  }
}

//----------------------------------------------------------------------------
// Move each cluster centroid along its normal onto the surface
void ProjectToSurface(const std::vector<double>& points,
                      const std::vector<vtkIdType>& faces,
                      const std::vector<double>& clusterNormals,
                      std::vector<double>& clusterCentroids)
{
  const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);
  const vtkIdType numberOfClusters = static_cast<vtkIdType>(clusterCentroids.size() / 3);

  const vtkIdType numberOfCandidates = std::min(numberOfFaces - 1, MAXIMUM_PROJECTION_CANDIDATES);
  if (numberOfCandidates < 1)
  {
    return;
  }
  const vtkIdType numberOfInitialCandidates = std::min(numberOfCandidates, INITIAL_PROJECTION_CANDIDATES);

  vtkNew<vtkDoubleArray> faceCentroidArray;
  faceCentroidArray->SetNumberOfComponents(3);
  faceCentroidArray->SetNumberOfTuples(numberOfFaces);
  double* faceCentroids = faceCentroidArray->GetPointer(0);
  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    const double* v0 = &points[faces[i * 3 + 0] * 3];
    const double* v1 = &points[faces[i * 3 + 1] * 3];
    const double* v2 = &points[faces[i * 3 + 2] * 3];
    double* centroid = &faceCentroids[i * 3];
    for (int k = 0; k < 3; ++k)
    {
      centroid[k] = (v0[k] + v1[k] + v2[k]) * VAL_1_3;
    }
  }
  vtkNew<vtkPoints> faceCentroidPoints;
  faceCentroidPoints->SetData(faceCentroidArray);
  vtkNew<vtkPolyData> faceCentroidPolyData;
  faceCentroidPolyData->SetPoints(faceCentroidPoints);
  vtkNew<vtkStaticPointLocator> locator;
  locator->SetDataSet(faceCentroidPolyData);
  locator->BuildLocator();

  double bounds[6];
  faceCentroidPoints->GetBounds(bounds);

  vtkNew<vtkIdList> closestIds;
  std::vector<vtkIdType> candidateFaces;
  std::vector<std::pair<double, vtkIdType>> distanceIds;
  for (vtkIdType i = 0; i < numberOfClusters; ++i)
  {
    double* centroid = &clusterCentroids[i * 3];
    const double* normal = &clusterNormals[i * 3];

    locator->FindClosestNPoints(static_cast<int>(numberOfInitialCandidates), centroid, closestIds);
    candidateFaces.assign(closestIds->begin(), closestIds->end());
    double distance = 0.0;
    bool hit = RayTrace(centroid, normal, points, faces, candidateFaces, distance);
    if (!hit && numberOfCandidates > numberOfInitialCandidates && !candidateFaces.empty())
    {
      // Faces are spread over a surface, so the radius that holds N faces grows with sqrt(N)
      const double initialRadius = std::sqrt(vtkMath::Distance2BetweenPoints(centroid, &faceCentroids[candidateFaces.back() * 3]))
                                   * std::sqrt(static_cast<double>(numberOfCandidates) / numberOfInitialCandidates);
      // Distance to the farthest corner of the bounding box, which contains every face centroid
      double farthest[3];
      for (int k = 0; k < 3; ++k)
      {
        farthest[k] = std::max(std::abs(centroid[k] - bounds[2 * k]), std::abs(centroid[k] - bounds[2 * k + 1]));
      }
      const double maximumRadius = Length(farthest) * (1.0 + 1e-6);
      FindClosestFaces(locator, faceCentroids, centroid, numberOfCandidates, initialRadius, maximumRadius, closestIds, distanceIds, candidateFaces);
      hit = RayTrace(centroid, normal, points, faces, candidateFaces, distance);
    }
    if (!hit)
    {
      distance = 0.0;
    }
    for (int k = 0; k < 3; ++k)
    {
      centroid[k] += normal[k] * distance;
    }
  }
}

//----------------------------------------------------------------------------
void CopyTriangles(vtkCellArray* cells, std::vector<vtkIdType>& faces)
{
  faces.clear();
  faces.reserve(cells->GetNumberOfCells() * 3);
  vtkSmartPointer<vtkCellArrayIterator> cellIterator = vtk::TakeSmartPointer(cells->NewIterator());
  for (cellIterator->GoToFirstCell(); !cellIterator->IsDoneWithTraversal(); cellIterator->GoToNextCell())
  {
    vtkIdType numberOfCellPoints = 0;
    const vtkIdType* cellPointIds = nullptr;
    cellIterator->GetCurrentCell(numberOfCellPoints, cellPointIds);
    if (numberOfCellPoints == 3)
    {
      faces.insert(faces.end(), cellPointIds, cellPointIds + 3);
    }
  }
}

//----------------------------------------------------------------------------
void SetPolyData(vtkPolyData* polyData, const std::vector<double>& points, const std::vector<vtkIdType>& faces)
{
  const vtkIdType numberOfPoints = static_cast<vtkIdType>(points.size() / 3);
  const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);

  vtkNew<vtkPoints> polyDataPoints;
  polyDataPoints->SetDataTypeToDouble();
  polyDataPoints->SetNumberOfPoints(numberOfPoints);
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
  {
    polyDataPoints->SetPoint(i, &points[i * 3]);
  }

  vtkNew<vtkCellArray> polys;
  polys->AllocateExact(numberOfFaces, numberOfFaces * 3);
  for (vtkIdType i = 0; i < numberOfFaces; ++i)
  {
    polys->InsertNextCell(3, &faces[i * 3]);
  }

  polyData->SetPoints(polyDataPoints);
  polyData->SetPolys(polys);
}

} // namespace

//----------------------------------------------------------------------------
vtkACVDRemeshFilter::vtkACVDRemeshFilter() = default;

//----------------------------------------------------------------------------
vtkACVDRemeshFilter::~vtkACVDRemeshFilter() = default;

//----------------------------------------------------------------------------
void vtkACVDRemeshFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "NumberOfClusters: " << this->NumberOfClusters << "\n";
  os << indent << "NumberOfSubdivisions: " << this->NumberOfSubdivisions << "\n";
  os << indent << "MaximumNumberOfIterations: " << this->MaximumNumberOfIterations << "\n";
  os << indent << "IsolatedClusterRemovalAttempts: " << this->IsolatedClusterRemovalAttempts << "\n";
  os << indent << "FastClustering: " << (this->FastClustering ? "On" : "Off") << "\n";
  os << indent << "ProjectPointsToSurface: " << (this->ProjectPointsToSurface ? "On" : "Off") << "\n";
  os << indent << "AlignNormals: " << (this->AlignNormals ? "On" : "Off") << "\n";
  os << indent << "CleanOutput: " << (this->CleanOutput ? "On" : "Off") << "\n";
}

//----------------------------------------------------------------------------
int vtkACVDRemeshFilter::RequestData(vtkInformation* vtkNotUsed(request), vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkPolyData* input = vtkPolyData::GetData(inputVector[0], 0);
  vtkPolyData* output = vtkPolyData::GetData(outputVector, 0);
  if (!input || !output)
  {
    return 0;
  }
  if (input->GetNumberOfPoints() == 0 || input->GetNumberOfPolys() + input->GetNumberOfStrips() == 0)
  {
    return 1;
  }

  try
  {
    // Intermediate meshes are released as soon as they are copied, to keep
    // the memory use low on large inputs.
    std::vector<double> points;
    std::vector<vtkIdType> faces;
    {
      // Triangulate the input, without its vertex and line cells
      vtkNew<vtkPolyData> inputCopy;
      inputCopy->ShallowCopy(input);
      vtkNew<vtkTriangleFilter> triangulator;
      triangulator->SetInputData(inputCopy);
      triangulator->PassVertsOff();
      triangulator->PassLinesOff();
      triangulator->Update();
      vtkPolyData* triangulated = triangulator->GetOutput();

      points.resize(triangulated->GetNumberOfPoints() * 3);
      for (vtkIdType i = 0; i < triangulated->GetNumberOfPoints(); ++i)
      {
        triangulated->GetPoint(i, &points[i * 3]);
      }
      CopyTriangles(triangulated->GetPolys(), faces);
    }

    for (int i = 0; i < this->NumberOfSubdivisions; ++i)
    {
      SubdivideTriangles(points, faces);
    }

    // Merge duplicate points. pyacvd does this after subdividing, even for 0 subdivisions.
    {
      vtkNew<vtkPolyData> workingMesh;
      SetPolyData(workingMesh, points, faces);
      std::vector<double>().swap(points);
      std::vector<vtkIdType>().swap(faces);
      vtkNew<vtkCleanPolyData> workingMeshCleaner;
      workingMeshCleaner->SetInputData(workingMesh);
      workingMeshCleaner->Update();
      workingMeshCleaner->SetInputData(nullptr);
      workingMesh->Initialize();
      vtkPolyData* cleanedMesh = workingMeshCleaner->GetOutput();

      // Degenerate triangles are converted to lines and vertices by the cleaner.
      // Those cells are ignored, and the points that only they use are removed.
      CopyTriangles(cleanedMesh->GetPolys(), faces);
      const vtkIdType numberOfCleanedPoints = cleanedMesh->GetNumberOfPoints();
      std::vector<vtkIdType> pointIdMap(numberOfCleanedPoints, -1);
      for (vtkIdType pointId : faces)
      {
        pointIdMap[pointId] = 0;
      }
      for (vtkIdType i = 0; i < numberOfCleanedPoints; ++i)
      {
        if (pointIdMap[i] == 0)
        {
          pointIdMap[i] = static_cast<vtkIdType>(points.size() / 3);
          double point[3];
          cleanedMesh->GetPoint(i, point);
          points.insert(points.end(), point, point + 3);
        }
      }
      for (vtkIdType& pointId : faces)
      {
        pointId = pointIdMap[pointId];
      }
    }
    if (faces.empty())
    {
      return 1;
    }
    const vtkIdType numberOfPoints = static_cast<vtkIdType>(points.size() / 3);

    // Clustering
    std::vector<double> area;
    std::vector<vtkIdType> clusters;
    vtkIdType numberOfClusters = 0;
    {
      std::vector<double> weightedPoints;
      PointWeights(points, faces, area, weightedPoints);
      std::replace(area.begin(), area.end(), 0.0, 1e-10);

      std::vector<vtkIdType> neighbors;
      std::vector<vtkIdType> offsets;
      NeighborsFromTriFaces(numberOfPoints, faces, neighbors, offsets);
      std::vector<vtkIdType> edges;
      UniqueEdges(neighbors, offsets, edges);

      if (this->NumberOfClusters > numberOfPoints)
      {
        clusters.resize(numberOfPoints);
        std::iota(clusters.begin(), clusters.end(), vtkIdType(0));
        numberOfClusters = numberOfPoints;
      }
      else if (this->FastClustering)
      {
        numberOfClusters = FastCluster(clusters, neighbors, offsets, area, edges, this->NumberOfClusters);
      }
      else
      {
        numberOfClusters = Cluster(clusters,
                                   neighbors,
                                   offsets,
                                   area,
                                   weightedPoints,
                                   edges,
                                   this->NumberOfClusters,
                                   this->MaximumNumberOfIterations,
                                   this->IsolatedClusterRemovalAttempts);
      }
    }

    // Area-weighted mean normal of each cluster
    std::vector<double> clusterNormals;
    if (this->ProjectPointsToSurface || this->AlignNormals)
    {
      std::vector<double> pointNormals;
      PointNormals(points, faces, pointNormals);
      clusterNormals.assign(numberOfClusters * 3, 0.0);
      for (vtkIdType i = 0; i < numberOfPoints; ++i)
      {
        for (int k = 0; k < 3; ++k)
        {
          clusterNormals[clusters[i] * 3 + k] += pointNormals[i * 3 + k] * area[i];
        }
      }
      for (vtkIdType i = 0; i < numberOfClusters; ++i)
      {
        double* normal = &clusterNormals[i * 3];
        double length = Length(normal);
        if (length == 0)
        {
          length = 1;
        }
        normal[0] /= length;
        normal[1] /= length;
        normal[2] /= length;
      }
    }

    // Area-weighted centroid of each cluster
    std::vector<double> clusterCentroids(numberOfClusters * 3, 0.0);
    std::vector<double> clusterArea(numberOfClusters, 0.0);
    for (vtkIdType i = 0; i < numberOfPoints; ++i)
    {
      clusterArea[clusters[i]] += area[i];
      for (int k = 0; k < 3; ++k)
      {
        clusterCentroids[clusters[i] * 3 + k] += points[i * 3 + k] * area[i];
      }
    }
    for (vtkIdType i = 0; i < numberOfClusters; ++i)
    {
      for (int k = 0; k < 3; ++k)
      {
        clusterCentroids[i * 3 + k] /= clusterArea[i];
      }
    }

    if (this->ProjectPointsToSurface)
    {
      ProjectToSurface(points, faces, clusterNormals, clusterCentroids);
    }

    // One output triangle for each distinct set of three clusters that share an input triangle
    const vtkIdType numberOfFaces = static_cast<vtkIdType>(faces.size() / 3);
    std::vector<std::array<vtkIdType, 3>> clusterFaces;
    for (vtkIdType i = 0; i < numberOfFaces; ++i)
    {
      std::array<vtkIdType, 3> clusterFace = { clusters[faces[i * 3]], clusters[faces[i * 3 + 1]], clusters[faces[i * 3 + 2]] };
      std::sort(clusterFace.begin(), clusterFace.end());
      if (clusterFace[0] != clusterFace[1] && clusterFace[1] != clusterFace[2])
      {
        clusterFaces.push_back(clusterFace);
      }
    }
    std::sort(clusterFaces.begin(), clusterFaces.end());
    clusterFaces.erase(std::unique(clusterFaces.begin(), clusterFaces.end()), clusterFaces.end());

    // Flip the triangles that face away from the mean normal of their clusters
    if (this->AlignNormals)
    {
      for (std::array<vtkIdType, 3>& clusterFace : clusterFaces)
      {
        double meanNormal[3];
        for (int k = 0; k < 3; ++k)
        {
          meanNormal[k] = clusterNormals[clusterFace[0] * 3 + k] + clusterNormals[clusterFace[1] * 3 + k] + clusterNormals[clusterFace[2] * 3 + k];
        }
        const double meanNormalLength = Length(meanNormal);
        for (int k = 0; k < 3; ++k)
        {
          meanNormal[k] /= meanNormalLength;
        }
        double faceNormal[3];
        FaceNormal(&clusterCentroids[clusterFace[0] * 3], &clusterCentroids[clusterFace[1] * 3], &clusterCentroids[clusterFace[2] * 3], faceNormal);
        if (meanNormal[0] * faceNormal[0] + meanNormal[1] * faceNormal[1] + meanNormal[2] * faceNormal[2] < 0.0)
        {
          std::swap(clusterFace[0], clusterFace[2]);
        }
      }
    }

    faces.resize(clusterFaces.size() * 3);
    for (size_t i = 0; i < clusterFaces.size(); ++i)
    {
      std::copy(clusterFaces[i].begin(), clusterFaces[i].end(), faces.begin() + i * 3);
    }
    vtkNew<vtkPolyData> remeshed;
    SetPolyData(remeshed, clusterCentroids, faces);

    if (this->CleanOutput)
    {
      vtkNew<vtkCleanPolyData> outputCleaner;
      outputCleaner->SetInputData(remeshed);
      outputCleaner->Update();
      output->ShallowCopy(outputCleaner->GetOutput());
    }
    else
    {
      output->ShallowCopy(remeshed);
    }
  }
  catch (const std::bad_alloc&)
  {
    vtkErrorMacro("RequestData: not enough memory to remesh the input. Try fewer subdivisions.");
    return 0;
  }

  return 1;
}
