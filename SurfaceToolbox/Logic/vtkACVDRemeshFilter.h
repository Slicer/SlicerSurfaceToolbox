/*==============================================================================

  Program: 3D Slicer

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file is a C++ port of pyacvd (https://github.com/pyvista/pyacvd),
  Copyright (c) 2017-2024 The PyVista Developers, MIT License.
  See vtkACVDRemeshFilter.cxx for the full license text.

==============================================================================*/

#ifndef vtkACVDRemeshFilter_h
#define vtkACVDRemeshFilter_h

#include "vtkSlicerSurfaceToolboxModuleLogicExport.h"

// VTK includes
#include <vtkPolyDataAlgorithm.h>

/// \brief Uniform surface remeshing using Approximated Centroidal Voronoi Diagrams (ACVD).
///
/// The points of the (optionally subdivided) input surface are grouped into
/// NumberOfClusters clusters of approximately equal area. Each cluster becomes
/// one output point and an output triangle is created wherever three clusters meet.
///
/// This is a C++ port of pyacvd (https://github.com/pyvista/pyacvd) that
/// produces the same output as
/// `Clustering(mesh).subdivide(n); cluster(k); create_mesh()`.
///
/// Polygons and triangle strips of the input are triangulated, vertex and line
/// cells are ignored. The output contains only points and triangles.
class VTK_SLICER_SURFACETOOLBOX_MODULE_LOGIC_EXPORT vtkACVDRemeshFilter : public vtkPolyDataAlgorithm
{
public:
  static vtkACVDRemeshFilter* New();
  vtkTypeMacro(vtkACVDRemeshFilter, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Desired number of points in the output mesh. Default is 10000.
  vtkSetClampMacro(NumberOfClusters, int, 1, VTK_INT_MAX);
  vtkGetMacro(NumberOfClusters, int);

  /// Number of times the input is linearly subdivided before clustering.
  /// Each subdivision splits every triangle into 4. It is needed when the input
  /// has fewer points than NumberOfClusters or has large triangles. Default is 0.
  vtkSetClampMacro(NumberOfSubdivisions, int, 0, VTK_INT_MAX);
  vtkGetMacro(NumberOfSubdivisions, int);

  /// Maximum number of iterations to make cluster sizes uniform. Default is 100.
  vtkSetClampMacro(MaximumNumberOfIterations, int, 0, VTK_INT_MAX);
  vtkGetMacro(MaximumNumberOfIterations, int);

  /// Number of attempts to remove disconnected parts of clusters. Default is 10.
  vtkSetClampMacro(IsolatedClusterRemovalAttempts, int, 0, VTK_INT_MAX);
  vtkGetMacro(IsolatedClusterRemovalAttempts, int);

  /// Only grow the initial clusters, without making their sizes uniform.
  /// Faster, but the output triangle sizes vary more. Default is off.
  vtkSetMacro(FastClustering, bool);
  vtkGetMacro(FastClustering, bool);
  vtkBooleanMacro(FastClustering, bool);

  /// Move the output points onto the input surface along the cluster normals.
  /// Default is on.
  vtkSetMacro(ProjectPointsToSurface, bool);
  vtkGetMacro(ProjectPointsToSurface, bool);
  vtkBooleanMacro(ProjectPointsToSurface, bool);

  /// Orient the output triangles to match the normals of the input surface.
  /// Default is on.
  vtkSetMacro(AlignNormals, bool);
  vtkGetMacro(AlignNormals, bool);
  vtkBooleanMacro(AlignNormals, bool);

  /// Merge coincident points and remove points that are not used by any
  /// output triangle. Default is on.
  vtkSetMacro(CleanOutput, bool);
  vtkGetMacro(CleanOutput, bool);
  vtkBooleanMacro(CleanOutput, bool);

protected:
  vtkACVDRemeshFilter();
  ~vtkACVDRemeshFilter() override;

  int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;

  int NumberOfClusters{ 10000 };
  int NumberOfSubdivisions{ 0 };
  int MaximumNumberOfIterations{ 100 };
  int IsolatedClusterRemovalAttempts{ 10 };
  bool FastClustering{ false };
  bool ProjectPointsToSurface{ true };
  bool AlignNormals{ true };
  bool CleanOutput{ true };

private:
  vtkACVDRemeshFilter(const vtkACVDRemeshFilter&) = delete;
  void operator=(const vtkACVDRemeshFilter&) = delete;
};

#endif
