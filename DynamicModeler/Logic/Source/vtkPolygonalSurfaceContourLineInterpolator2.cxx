/*=========================================================================

  Copyright (c) Karthik Krishnan
  See Copyright.txt for details.

=========================================================================*/

#include "vtkPolygonalSurfaceContourLineInterpolator2.h"

#include "vtkObjectFactory.h"
#include "vtkContourRepresentation.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkPointData.h"
#include "vtkCellArray.h"
#include "vtkCell.h"
#include "vtkMath.h"
#include "vtkCellLocator.h"
#include "vtkPolygonalSurfacePointPlacer.h"
#include "vtkIdList.h"
#include "vtkDijkstraGraphGeodesicPath.h"
#include "vtkFastMarchingGeodesicPath.h"
#include "vtkSmartPointer.h"
#include "vtkNew.h"

vtkStandardNewMacro(vtkPolygonalSurfaceContourLineInterpolator2);

//----------------------------------------------------------------------
vtkPolygonalSurfaceContourLineInterpolator2
::vtkPolygonalSurfaceContourLineInterpolator2()
{
  this->LastInterpolatedVertexIds[0] = -1;
  this->LastInterpolatedVertexIds[1] = -1;
  this->DistanceOffset              = 0.0;
  this->GeodesicPath                 = vtkDijkstraGraphGeodesicPath::New();
  this->GeodesicMethod               = DijkstraMethod;
}

//----------------------------------------------------------------------
vtkPolygonalSurfaceContourLineInterpolator2
::~vtkPolygonalSurfaceContourLineInterpolator2()
{
  this->GeodesicPath->Delete();
}

//----------------------------------------------------------------------
int vtkPolygonalSurfaceContourLineInterpolator2::UpdateNode(
    vtkRenderer *, vtkContourRepresentation *,
    double * vtkNotUsed(node), int vtkNotUsed(idx) )
{
  return 0;
}

//----------------------------------------------------------------------
int vtkPolygonalSurfaceContourLineInterpolator2::InterpolateLine(
                          vtkRenderer *,
                          vtkContourRepresentation *rep,
                          int idx1, int idx2 )
{
  vtkPolygonalSurfacePointPlacer *placer =
    vtkPolygonalSurfacePointPlacer::SafeDownCast(rep->GetPointPlacer());
  if (!placer)
    {
    return 1;
    }

  double p1[3], p2[3], p[3];
  rep->GetNthNodeWorldPosition( idx1, p1 );
  rep->GetNthNodeWorldPosition( idx2, p2 );

  typedef vtkPolygonalSurfacePointPlacer::Node NodeType;
  NodeType *nodeBegin = placer->GetNodeAtWorldPosition(p1);
  NodeType *nodeEnd   = placer->GetNodeAtWorldPosition(p2);
  if (nodeBegin->PolyData != nodeEnd->PolyData)
    {
    return 1;
    }

  // Find the starting and ending point id's
  vtkIdType beginVertId = -1, endVertId = -1;
  double minDistance;
  if (nodeBegin->CellId == -1)
    {
    // If no cell is specified, use the pointid instead
    beginVertId = nodeBegin->PointId;
    }
  else
    {
    vtkCell *cellBegin = nodeBegin->PolyData->GetCell(nodeBegin->CellId);
    vtkPoints *cellBeginPoints = cellBegin->GetPoints();

    minDistance = VTK_DOUBLE_MAX;
    for (int i = 0; i < cellBegin->GetNumberOfPoints(); i++)
      {
      cellBeginPoints->GetPoint(i, p);
      double distance = vtkMath::Distance2BetweenPoints( p, p1 );
      if (distance < minDistance)
        {
        beginVertId = cellBegin->GetPointId(i);
        minDistance = distance;
        }
      }
    }

    if (nodeEnd->CellId == -1)
      {
      // If no cell is specified, use the pointid instead
      endVertId = nodeEnd->PointId;
      }
    else
      {
      vtkCell *cellEnd   = nodeEnd->PolyData->GetCell(nodeEnd->CellId);
      vtkPoints *cellEndPoints   = cellEnd->GetPoints();

      minDistance = VTK_DOUBLE_MAX;
      for (int i = 0; i < cellEnd->GetNumberOfPoints(); i++)
        {
        cellEndPoints->GetPoint(i, p);
        double distance = vtkMath::Distance2BetweenPoints( p, p2 );
        if (distance < minDistance)
          {
          endVertId = cellEnd->GetPointId(i);
          minDistance = distance;
          }
        }
      }

  if (beginVertId == -1 || endVertId == -1)
    {
    // Could not find the starting and ending cells. We can't interpolate.
    return 0;
    }

  vtkSmartPointer< vtkIdList > vertexIds = NULL;

  vtkDijkstraGraphGeodesicPath *dggp =
      vtkDijkstraGraphGeodesicPath::SafeDownCast(this->GeodesicPath);
  vtkFastMarchingGeodesicPath *fmgp =
      vtkFastMarchingGeodesicPath::SafeDownCast(this->GeodesicPath);

  if (this->GeodesicMethod ==
      vtkPolygonalSurfaceContourLineInterpolator2::DijkstraMethod)
    {
    // Compute the shortest path through the surface mesh along its edges
    // using Dijkstra.

#if (VTK_MAJOR_VERSION < 6)
    dggp->SetInput( nodeBegin->PolyData );
#else
    dggp->SetInputData( nodeBegin->PolyData );
#endif
    dggp->SetStartVertex( endVertId );
    dggp->SetEndVertex( beginVertId );
    dggp->Update();
    vertexIds = dggp->GetIdList();
    }
  else // fast marching
    {
    // Compute the shortest path through the surface mesh using fast marching

#if (VTK_MAJOR_VERSION < 6)
    fmgp->SetInput( nodeBegin->PolyData );
#else
    fmgp->SetInputData( nodeBegin->PolyData );
#endif
    fmgp->SetBeginPointId( beginVertId );
    vtkNew< vtkIdList > destinationSeeds;
    destinationSeeds->InsertNextId( endVertId );
    fmgp->SetSeeds( destinationSeeds.GetPointer() );
    fmgp->SetInterpolationOrder(this->InterpolationOrder);
    fmgp->Update();

    // Get the ids of points on the mesh closest to the path. In the case of
    // 0th order, we avoid repeats in ptIds
    if (this->InterpolationOrder == 0)
      {
      vertexIds = fmgp->GetZerothOrderPathPointIds();
      }
    else
      {
      // Copy the non-unique closest vertex ids from the first order point ids
      vtkIdList *fids = fmgp->GetFirstOrderPathPointIds();
      vertexIds = vtkSmartPointer< vtkIdList >::New();
      int nIds = fids->GetNumberOfIds()/2;
      vertexIds->SetNumberOfIds(nIds);
      for (vtkIdType i = 0; i < nIds; i++)
        {
        vertexIds->SetId(i, fids->GetId(2*i));
        }
      }
    }

  vtkPolyData *pd = this->GeodesicPath->GetOutput();

  // We assume there's only one cell of course
  vtkIdType npts = 0, *pts = NULL;
  pd->GetLines()->InitTraversal();
  pd->GetLines()->GetNextCell( npts, pts );

  // Get the vertex normals if there is a height offset. The offset at
  // each node of the graph is in the direction of the vertex normal.

  double vertexNormal[3];
  vtkDataArray *vertexNormals = NULL;
  if (this->DistanceOffset != 0.0)
    {
    vertexNormals = nodeBegin->PolyData->GetPointData()->GetNormals();
    }

  for (int n = 0; n < npts; n++)
    {
    pd->GetPoint( pts[n], p );

    // This is the id of the closest point on the polygonal surface.
    const vtkIdType ptId = vertexIds->GetId(n);

    // Offset the point in the direction of the normal, if a distance
    // offset is specified.
    if (vertexNormals)
      {
      vertexNormals->GetTuple( ptId, vertexNormal );
      p[0] += vertexNormal[0] * this->DistanceOffset;
      p[1] += vertexNormal[1] * this->DistanceOffset;
      p[2] += vertexNormal[2] * this->DistanceOffset;
      }

    // Add this point as an intermediate node of the contour. Store tehe
    // ptId if necessary.
    rep->AddIntermediatePointWorldPosition( idx1, p, ptId );
    }

  this->LastInterpolatedVertexIds[0] = beginVertId;
  this->LastInterpolatedVertexIds[1] = endVertId;

  // Also set the start and end node on the contour rep
  rep->GetNthNode(idx1)->PointId = beginVertId;
  rep->GetNthNode(idx2)->PointId = endVertId;

  return 1;
}

//----------------------------------------------------------------------
void vtkPolygonalSurfaceContourLineInterpolator2
::GetContourPointIds( vtkContourRepresentation *rep, vtkIdList *ids )
{
  // Get the number of points in the contour and pre-allocate size

  const int nNodes = rep->GetNumberOfNodes();

  vtkIdType nPoints = 0;
  for (int i = 0; i < nNodes; i++)
    {
    // 1 for the node and then the number of points.
    nPoints += (rep->GetNthNode(i)->Points.size() + 1);
    }

  ids->SetNumberOfIds(nPoints);

  // Now fill the point ids

  int idx = 0;
  for (int i = 0; i < nNodes; i++)
    {
    vtkContourRepresentationNode *node = rep->GetNthNode(i);
    ids->SetId(idx++, node->PointId);
    const int nIntermediatePts = static_cast< int >(node->Points.size());

    for (int j = 0; j < nIntermediatePts; j++)
      {
      ids->SetId(idx++, node->Points[j]->PointId);
      }
    }
}

//----------------------------------------------------------------------
void vtkPolygonalSurfaceContourLineInterpolator2
::SetGeodesicMethodToFastMarching()
{
  this->SetGeodesicMethod( (int)
      vtkPolygonalSurfaceContourLineInterpolator2::FastMarchingMethod );
}

//----------------------------------------------------------------------
void vtkPolygonalSurfaceContourLineInterpolator2
::SetGeodesicMethodToDijkstra()
{
  this->SetGeodesicMethod( (int)
      vtkPolygonalSurfaceContourLineInterpolator2::DijkstraMethod );
}

//----------------------------------------------------------------------
void vtkPolygonalSurfaceContourLineInterpolator2::SetGeodesicMethod( int m )
{
  if (m == this->GeodesicMethod)
    {
    return;
    }

  // Instantiate the algorithm of the right type

  if (this->GeodesicPath)
    {
    this->GeodesicPath->Delete();
    this->GeodesicPath = NULL;
    }

  if (m == vtkPolygonalSurfaceContourLineInterpolator2::DijkstraMethod)
    {
    this->GeodesicPath = vtkDijkstraGraphGeodesicPath::New();
    }
  else
    {
    this->GeodesicPath = vtkFastMarchingGeodesicPath::New();
    }

  this->GeodesicMethod = m;
  this->Modified();
}

//----------------------------------------------------------------------
void vtkPolygonalSurfaceContourLineInterpolator2::PrintSelf(
                              ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "DistanceOffset: " << this->DistanceOffset << endl;
  os << indent << "InterpolationOrder: " << this->InterpolationOrder << endl;
  os << indent << "GeodesicPath: " << this->GeodesicPath << endl;
  // LastInterpolatedVertexIds
}
