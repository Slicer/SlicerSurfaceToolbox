/*=========================================================================

  Copyright (c) Karthik Krishnan
  See Copyright.txt for details.

=========================================================================*/
// .NAME vtkPolygonalSurfaceContourLineInterpolator2 - Interpolate path on a surface mesh.
//
// .SECTION Description
// vtkPolygonalSurfaceContourLineInterpolator2 interpolates and places
// contour points on polygonal surfaces. The class interpolates nodes by
// computing a geodesic path through a surface mesh. Two path computation
// methods are supported: (a) The fast marching method. This computes the
// distance via fast marching from the destination node. Distances are computed
// on the vertices of the mesh. The distance field is stopped when it reaches
// the source node. The path is then computed from the source to the
// destination by traversing along the gradient of the field. The user may
// choose to have the path be linearly interpolated (ie it can lie in between
// mesh vertices) or it can be constrained to lie on vertices of the mesh,
// traversing along its edges.
// (b) The Dijkstra method. A shortest path is computed from the destination to
// the source using the Dijkstra algorithm, treating edges in the mesh as a
// graph. The path lies on vertices of the mesh, traversing along its edges.
// vtkDijkstraGraphGeodesicPath.
//
// The class is mean to be used in conjunction with
// vtkPolygonalSurfacePointPlacer. The reason for this weak coupling is a
// performance issue, both classes need to perform a cell pick, and
// coupling avoids multiple cell picks (cell picks are slow).
//
// The class has an option that allows you to raise/lower the path with
// respect to the surface mesh, (eg the path can lie 10ft above the surface).
// In these cases the offset is computed by raising the path in the direction
// of the point normal. If you choose to use this option, you should have
// normals computed on the mesh (pass it through vtkPolyDataNormals with
// splitting turned off).
//
// .SECTION Caveats
// This class works only on triangle meshes. Meshes must be manifold. 
//
// .SECTION See Also
// vtkDijkstraGraphGeodesicPath, vtkFastMarchingGeodesicDistance,
//vtkFastMarchingGeodesicPath

#ifndef __vtkPolygonalSurfaceContourLineInterpolator2_h
#define __vtkPolygonalSurfaceContourLineInterpolator2_h

#include "vtkPolyDataContourLineInterpolator.h"

class vtkGeodesicPath;
class vtkIdList;

class VTK_EXPORT vtkPolygonalSurfaceContourLineInterpolator2 : public vtkPolyDataContourLineInterpolator
{
public:
  // Description:
  // Standard methods for instances of this class.
  vtkTypeMacro(vtkPolygonalSurfaceContourLineInterpolator2,
               vtkPolyDataContourLineInterpolator);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  static vtkPolygonalSurfaceContourLineInterpolator2 *New();

  // Description:
  // Interpolation order of the path traced through the surface mesh. A zeroth
  // order path passes through vertices of the mesh. A first order path passes
  // in between vertices. Each point in the first order path is guarenteed to
  // lie on an edge. Default is zeroth order. Linear interpolation can be used
  // only with the fast marching method.
  vtkSetClampMacro( InterpolationOrder, int, 0, 1 );
  vtkGetMacro( InterpolationOrder, int );

  // Description:
  // Method used to compute the geodesic path
  //BTX
  typedef enum
    {
    DijkstraMethod = 0,
    FastMarchingMethod
    }  GeodesicMethodType;
  //ETX

  // Description:
  // Method used to compute the geodesic path
  virtual void SetGeodesicMethodToDijkstra();
  virtual void SetGeodesicMethodToFastMarching();
  vtkGetMacro( GeodesicMethod, int );
  virtual void SetGeodesicMethod( int );

  // Description:
  // Subclasses that wish to interpolate a line segment must implement this.
  // For instance vtkBezierContourLineInterpolator adds nodes between idx1
  // and idx2, that allow the contour to adhere to a bezier curve.
  int InterpolateLine( vtkRenderer *ren, vtkContourRepresentation *rep,
                       int idx1, int idx2 ) override;

  // Description:
  // The interpolator is given a chance to update the node.
  // vtkImageContourLineInterpolator updates the idx'th node in the contour,
  // so it automatically sticks to edges in the vicinity as the user
  // constructs the contour.
  // Returns 0 if the node (world position) is unchanged.
  int UpdateNode( vtkRenderer *, vtkContourRepresentation *,
                  double * vtkNotUsed(node), int vtkNotUsed(idx) ) override;

  // Description:
  // Height offset at which points may be placed on the polygonal surface.
  // If you specify a non-zero value here, be sure to have computed vertex
  // normals on your input polygonal data. (easily done with
  // vtkPolyDataNormals. Be sure to turn VertexSplitting off.).
  vtkSetMacro( DistanceOffset, double );
  vtkGetMacro( DistanceOffset, double );

  // Description:
  // Get the contour point ids. These point ids correspond to those on the
  // polygonal surface. If linear interpolation is used to compute the path,
  // the closest point ids are returned.
  void GetContourPointIds( vtkContourRepresentation *rep, vtkIdList *idList );

protected:
  vtkPolygonalSurfaceContourLineInterpolator2();
  ~vtkPolygonalSurfaceContourLineInterpolator2();

  // Description:
  // Draw the polyline at a certain height (in the direction of the vertex
  // normal) above the polydata.
  double DistanceOffset;

  // Desciption:
  // The geodesic method
  int GeodesicMethod;

  // Description:
  // Path interpolation order
  int InterpolationOrder;

private:
  vtkPolygonalSurfaceContourLineInterpolator2(const vtkPolygonalSurfaceContourLineInterpolator2&);  //Not implemented
  void operator=(const vtkPolygonalSurfaceContourLineInterpolator2&);  //Not implemented

  // Cache the last used vertex id's (start and end).
  // If they are the same, don't recompute.
  vtkIdType      LastInterpolatedVertexIds[2];

  vtkGeodesicPath* GeodesicPath;
};

#endif
