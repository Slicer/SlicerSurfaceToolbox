/*=========================================================================

  Copyright (c) Karthik Krishnan
  See Copyright.txt for details.

=========================================================================*/

// .NAME vtkPolyDataGeodesicDistance - Abstract base for classes that generate a geodesic path
// .SECTION Description
// Serves as a base class for algorithms that trace a geodesic path on a
// polygonal dataset.

#ifndef __vtkPolyDataGeodesicDistance_h
#define __vtkPolyDataGeodesicDistance_h

#include "vtkPolyDataAlgorithm.h"

class vtkPolyData;
class vtkFloatArray;
class vtkIdList;

class VTK_EXPORT vtkPolyDataGeodesicDistance : public vtkPolyDataAlgorithm
{
public:

  // Description:
  // Standard methids for printing and determining type information.
  vtkTypeMacro(vtkPolyDataGeodesicDistance,vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  // Description:
  // A list of points (seeds) on the input mesh from which to perform fast
  // marching. These are pointIds from the input mesh. At least one seed must
  // be specified.
  virtual void SetSeeds( vtkIdList * );
  vtkGetObjectMacro( Seeds, vtkIdList );

  // Description:
  // Set/Get the name of the distance field data array that this class will
  // crate. This is a scalar floating precision field array representing the
  // geodesic distance from the vertex. If not set, this class will not
  // generate a distance field on the output. This may be useful for
  // interactive applications, which may set a termination criteria and want
  // just the visited point ids and their distances.
  vtkSetStringMacro(FieldDataName);
  vtkGetStringMacro(FieldDataName);

  // Overload GetMTime() because we depend on seeds
  vtkMTimeType GetMTime() override;

protected:
  vtkPolyDataGeodesicDistance();
  ~vtkPolyDataGeodesicDistance();

  // Get the distance field array on the polydata
  vtkFloatArray *GetGeodesicDistanceField(vtkPolyData *pd);

  // Compute the geodesic distance. Subclasses should override this method.
  // Returns 1 on success;
  virtual int Compute();

  char      * FieldDataName;
  vtkIdList * Seeds;

private:
  vtkPolyDataGeodesicDistance(const vtkPolyDataGeodesicDistance&);  // Not implemented.
  void operator=(const vtkPolyDataGeodesicDistance&);  // Not implemented.
};

#endif

