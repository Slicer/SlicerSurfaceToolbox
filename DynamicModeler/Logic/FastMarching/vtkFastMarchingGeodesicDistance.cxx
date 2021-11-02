/*=========================================================================

  Copyright (c) Karthik Krishnan
  See Copyright.txt for details.

=========================================================================*/

#include "vtkFastMarchingGeodesicDistance.h"

#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkExecutive.h"
#include "vtkIdList.h"
#include "vtkFloatArray.h"
#include "vtkCellArray.h"
#include "vtkCommand.h"

#include "GW_GeodesicMesh.h"
#include "GW_GeodesicPath.h"
#include "GW_Vertex.h"
#include "GW_Face.h"
#include <assert.h>
#include <set>

#ifdef _WIN32
// new is being defined to a new method that takes in 4 parameters.
// Go back to what its supposed to be !
#ifdef new
#undef new
#endif
#endif


//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkFastMarchingGeodesicDistance);
vtkCxxSetObjectMacro(vtkFastMarchingGeodesicDistance, DestinationVertexStopCriterion, vtkIdList);
vtkCxxSetObjectMacro(vtkFastMarchingGeodesicDistance,
                     ExclusionPointIds, vtkIdList);
vtkCxxSetObjectMacro(vtkFastMarchingGeodesicDistance,
                     PropagationWeights, vtkDataArray);

//-----------------------------------------------------------------------------
class vtkGeodesicMeshInternals
{
public:
  vtkGeodesicMeshInternals()
    {
    this->Mesh = NULL;
    }

  ~vtkGeodesicMeshInternals()
    {
    if (this->Mesh)
      {
      delete this->Mesh;
      }
    }

  // This callback is called every time a front vertex is visited to check
  // if we should terminate marching.
  static GW::GW_Bool FastMarchingStopCallback(
      GW::GW_GeodesicVertex& v, void *callbackData )
    {
    vtkFastMarchingGeodesicDistance *filter =
      static_cast< vtkFastMarchingGeodesicDistance* >(callbackData);

    // Stop if the vertex is farther than the distance stop criteria
    if (filter->DistanceStopCriterion > 0)
      {
      return (filter->DistanceStopCriterion <= v.GetDistance());
      }

    // Stop if the vertex id is one of the destination vertices
    if (filter->DestinationVertexStopCriterion->GetNumberOfIds())
      {
      if (filter->DestinationVertexStopCriterion->IsId(v.GetID()) != -1)
        {
        return true;
        }
      }

    return false;
    }


  // This callback is invoked prior to adding new vertices to the front
  static GW::GW_Bool FastMarchingVertexInsertionCallback(
      GW::GW_GeodesicVertex& v, GW::GW_Float vtkNotUsed(distance), void *callbackData )
    {
    vtkFastMarchingGeodesicDistance *filter =
      static_cast< vtkFastMarchingGeodesicDistance* >(callbackData);

    // Prevent bleeding into exclusion regions
    if (filter->ExclusionPointIds->GetNumberOfIds())
      {
      if (filter->ExclusionPointIds->IsId(v.GetID()) != -1)
        {
        // do not add it.
        return false;
        }
      }

    return true;
    }

  // This callback is invoked to get the propagation weight at a given vertex.
  // The default (if not specified) is a constant weight of 1 everywhere.
  static GW::GW_Float FastMarchingPropagationWeightCallback(
      GW::GW_GeodesicVertex& v, void *callbackData )
    {
    vtkFastMarchingGeodesicDistance *filter =
      static_cast< vtkFastMarchingGeodesicDistance* >(callbackData);

    return (GW::GW_Float)filter->PropagationWeights->GetTuple1(v.GetID());
    }

  // This callback is invoked to get the propagation weight at a given vertex.
  // Th result is no weight == 1
  static inline GW::GW_Float FastMarchingPropagationNoWeightCallback(
      GW::GW_GeodesicVertex&, void * )
    {
    return 1.0;
    }

  GW::GW_GeodesicMesh *Mesh;
};


//-----------------------------------------------------------------------------
vtkFastMarchingGeodesicDistance::vtkFastMarchingGeodesicDistance()
{
  this->Internals = new vtkGeodesicMeshInternals;
  this->MaximumDistance = 0;
  this->NotVisitedValue = -1;
  this->NumberOfVisitedPoints = 0;
  this->DistanceStopCriterion = -1;
  this->DestinationVertexStopCriterion = NULL;
  this->ExclusionPointIds = NULL;
  this->PropagationWeights = NULL;
  this->IterationIndex = 0;
  this->FastMarchingIterationEventResolution = 100;
}

//-----------------------------------------------------------------------------
vtkFastMarchingGeodesicDistance::~vtkFastMarchingGeodesicDistance()
{
  this->SetDestinationVertexStopCriterion(NULL);
  this->SetExclusionPointIds(NULL);
  this->SetPropagationWeights(NULL);
  delete this->Internals;
}

//----------------------------------------------------------------------------
int vtkFastMarchingGeodesicDistance::RequestData(
  vtkInformation *           vtkNotUsed( request ),
  vtkInformationVector **    inputVector,
  vtkInformationVector *     outputVector)
{
  vtkInformation * inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  vtkPolyData *input = vtkPolyData::SafeDownCast(
    inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData *output = vtkPolyData::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!output || !input)
    {
    return 0;
    }

  // Copy everything from the input
  output->ShallowCopy(input);

  // Initialize the GW_GeodesicMesh structure
  this->SetupGeodesicMesh(input);

  // Setup termination criteria, if any
  this->SetupCallbacks();

  // Seed
  this->AddSeeds();

  // Do the fast marching
  this->Compute();

  // Copy the distance field onto the output
  this->CopyDistanceField(output);

  return 1;
}

//-----------------------------------------------------------------------------
void vtkFastMarchingGeodesicDistance::SetupGeodesicMesh( vtkPolyData *in )
{
  if (this->GeodesicMeshBuildTime.GetMTime() < in->GetMTime()
      || !this->Internals->Mesh)
    {
    // Need to rebuild the GW_GeodesicMesh

    if (!this->Internals->Mesh)
      {
      delete this->Internals->Mesh;
      this->Internals->Mesh = new GW::GW_GeodesicMesh();
      this->Internals->Mesh->SetCallbackData(this);
      }

    // Setup the GW_GeodesicMesh mesh
    GW::GW_GeodesicMesh *mesh = this->Internals->Mesh;

    // Setup the mesh points
    double pt[3];
    vtkPoints *pts = in->GetPoints();
    const int nPts = in->GetNumberOfPoints();
    mesh->SetNbrVertex(nPts);

    for (int i=0; i < nPts; i++) // loop over the points and copy them over
      {
      pts->GetPoint(i, pt);
      GW::GW_GeodesicVertex & point =
          (GW::GW_GeodesicVertex &)mesh->CreateNewVertex();
      point.SetPosition( GW::GW_Vector3D( pt[0], pt[1], pt[2] ) );
      mesh->SetVertex( i, &point );
      }

    const vtkIdType* ptIds = nullptr;
    vtkIdType npts = 0;
    const int nCells = in->GetNumberOfPolys();
    vtkCellArray *cells = in->GetPolys();
    if (!cells)
      {
      return;
      }
    cells->InitTraversal();

    mesh->SetNbrFace(nCells);
    for ( int i = 0; i < nCells; i++)
      {
      // Possible types
      //    VTK_VERTEX, VTK_POLY_VERTEX, VTK_LINE,
      //    VTK_POLY_LINE,VTK_TRIANGLE, VTK_QUAD,
      //    VTK_POLYGON, or VTK_TRIANGLE_STRIP.

      // only handle triangles
      cells->GetNextCell(npts, ptIds);

      // bail out
      if (npts != 3)
        {
        vtkErrorMacro( << "This filter can only work with triangle meshes." );
        delete this->Internals->Mesh;
        this->Internals->Mesh = NULL;
        return;
        }

      GW::GW_GeodesicFace& cell = (GW::GW_GeodesicFace &) mesh->CreateNewFace();
      GW::GW_Vertex* a = mesh->GetVertex(ptIds[0]);
      GW::GW_Vertex* b = mesh->GetVertex(ptIds[1]);
      GW::GW_Vertex* c = mesh->GetVertex(ptIds[2]);
      cell.SetVertex( *a, *b, *c );
      mesh->SetFace( i, &cell );
      }

    mesh->BuildConnectivity();

    this->GeodesicMeshBuildTime.Modified();
    }

  // Restart in preparation for fast marching
  this->Internals->Mesh->ResetGeodesicMesh();
}

//-----------------------------------------------------------------------------
void vtkFastMarchingGeodesicDistance::AddSeeds()
{
  if (!this->Seeds || !this->Seeds->GetNumberOfIds())
    {
    vtkErrorMacro( << "Please supply at least one seed." );
    return;
    }

  const int n = this->Seeds->GetNumberOfIds();
  GW::GW_GeodesicMesh *mesh = this->Internals->Mesh;
  for (int i = 0; i < n; i++)
    {
    mesh->AddStartVertex( *((GW::GW_GeodesicVertex*)mesh->
        GetVertex((GW::GW_U32)(this->Seeds->GetId(i)))));
    }
}

//-----------------------------------------------------------------------------
int vtkFastMarchingGeodesicDistance::Compute()
{
  this->MaximumDistance = 0;

  this->Internals->Mesh->SetUpFastMarching();

  // Do the fast marching

  while( !this->Internals->Mesh->PerformFastMarchingOneStep() )
	{
    if ((++this->IterationIndex) %
          this->FastMarchingIterationEventResolution == 0)
      {
      this->InvokeEvent(vtkFastMarchingGeodesicDistance::IterationEvent);
      }
    }

  return 1;
}

//-----------------------------------------------------------------------------
void vtkFastMarchingGeodesicDistance::CopyDistanceField(vtkPolyData *pd)
{
  GW::GW_GeodesicMesh *mesh = this->Internals->Mesh;

  float distance;
  this->MaximumDistance = 0;
  this->NumberOfVisitedPoints = 0;

  const int n = mesh->GetNbrVertex();
  vtkFloatArray *arr = this->GetGeodesicDistanceField(pd);

  for (int i = 0; i < n; i++)
    {
    GW::GW_GeodesicVertex* vertex =
     (GW::GW_GeodesicVertex*)(mesh->GetVertex((GW::GW_U32)i));

    if (vertex->GetState() > 1)
      {
      // This point is in the traversal list
      ++this->NumberOfVisitedPoints;
      distance = vertex->GetDistance();
      if (distance > this->MaximumDistance)
        {
        this->MaximumDistance = distance;
        }

      if (arr)
        {
        arr->SetValue(i, distance);
        }
      }
    else
      {
      if (arr)
        {
        // Haven't been to this point yet
        arr->SetValue(i, this->NotVisitedValue);
        }
      }

    } // end loop over all vertices
}

//-----------------------------------------------------------------------------
void vtkFastMarchingGeodesicDistance::SetupCallbacks()
{
  // Setup termination criteria
  if (this->DistanceStopCriterion > 0 ||
      (this->DestinationVertexStopCriterion &&
       this->DestinationVertexStopCriterion->GetNumberOfIds()))
    {
    this->Internals->Mesh->RegisterForceStopCallbackFunction(
          vtkGeodesicMeshInternals::FastMarchingStopCallback);
    }
  else
    {
    this->Internals->Mesh->RegisterForceStopCallbackFunction(NULL);
    }

  // Setup callback prior to adding a new vertex into the front
  if (this->ExclusionPointIds && this->ExclusionPointIds->GetNumberOfIds())
    {
    this->Internals->Mesh->RegisterVertexInsersionCallbackFunction(
      vtkGeodesicMeshInternals::FastMarchingVertexInsertionCallback);
    }
  else
    {
    this->Internals->Mesh->RegisterVertexInsersionCallbackFunction(NULL);
    }


  // Setup callback to get the propagation weights
  if (this->PropagationWeights &&
        this->PropagationWeights->GetNumberOfTuples() ==
            this->Internals->Mesh->GetNbrVertex())
    {
    this->Internals->Mesh->RegisterWeightCallbackFunction(
      vtkGeodesicMeshInternals::FastMarchingPropagationWeightCallback);
    }
  else
    {
    this->Internals->Mesh->RegisterWeightCallbackFunction(
      vtkGeodesicMeshInternals::FastMarchingPropagationNoWeightCallback);
    }
}

//-----------------------------------------------------------------------------
void* vtkFastMarchingGeodesicDistance::GetGeodesicMesh()
{
  return this->Internals->Mesh;
}

/*
//-----------------------------------------------------------------------------
void vtkFastMarchingGeodesicDistance::SetPropagationWeights(vtkDoubleArray *wts)
{
  // cast from double to float (as expected by GW::GeodesicMesh) and copy
  this->SetPropagationWeights(NULL);
  vtkFloatArray *farr = vtkFloatArray::New();
  int n = wts->GetNumberOfTuples();
  farr->SetNumberOfTuples(n);
  double *wt = wts->GetPointer(0);
  float *castedWt = farr->GetPointer(0);
  for (int i = 0; i < n; ++i, ++wt, ++castedWt)
    {
    *castedWt = static_cast< float >(*wt);
    }
  this->SetPropagationWeights(farr);
  farr->Delete();
}
*/

//-----------------------------------------------------------------------------
void vtkFastMarchingGeodesicDistance::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "MaximumDistance: " << this->MaximumDistance << endl;
  os << indent << "NotVisitedValue: " << this->NotVisitedValue << endl;
  os << indent << "NumberOfVisitedPoints: "
     << this->NumberOfVisitedPoints << endl;
  os << indent << "DistanceStopCriterion: "
     << this->DistanceStopCriterion << endl;
  os << indent << "DestinationVertexStopCriterion: "
     << this->DestinationVertexStopCriterion << endl;
  if (this->DestinationVertexStopCriterion)
    {
    this->DestinationVertexStopCriterion->PrintSelf(os, indent.GetNextIndent());
    }
  os << indent << "ExclusionPointIds: " << this->ExclusionPointIds << endl;
  if (this->ExclusionPointIds)
    {
    this->ExclusionPointIds->PrintSelf(os, indent.GetNextIndent());
    }
  os << indent << "PropagationWeights: " << this->ExclusionPointIds << endl;
  if (this->PropagationWeights)
    {
    this->PropagationWeights->PrintSelf(os, indent.GetNextIndent());
    }
  os << indent << "MaximumDistance: " << this->MaximumDistance << endl;
  os << indent << "NotVisitedValue: " << this->NotVisitedValue << endl;
  os << indent << "NumberOfVisitedPoints: "
     << this->NumberOfVisitedPoints << endl;
  os << indent << "FastMarchingIterationEventResolution: "
     << this->FastMarchingIterationEventResolution << endl;
  os << indent << "IterationIndex: " << this->IterationIndex << endl;
  // GeodesicMeshBuildTime
}
