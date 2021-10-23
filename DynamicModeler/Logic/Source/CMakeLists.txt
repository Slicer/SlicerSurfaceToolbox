project( vtkMeshGeodesics )

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

set( vtkMeshGeodesics_SRCS
  vtkPolyDataGeodesicDistance.cxx
  vtkFastMarchingGeodesicDistance.cxx
  vtkFastMarchingGeodesicPath.cxx
  vtkPolygonalSurfaceContourLineInterpolator2.cxx )
set( vtkMeshGeodesics_HDRS
  vtkPolyDataGeodesicDistance.h
  vtkFastMarchingGeodesicDistance.h
  vtkFastMarchingGeodesicPath.h
  vtkPolygonalSurfaceContourLineInterpolator2.h )

include_directories( 
  ${CMAKE_CURRENT_SOURCE_DIR}/../FastMarching 
  ${CMAKE_CURRENT_SOURCE_DIR}/../FastMarching/gw_core
  ${CMAKE_CURRENT_SOURCE_DIR}/../FastMarching/gw_geodesic
)

add_library( vtkMeshGeodesics
  ${vtkMeshGeodesics_SRCS}
  ${vtkMeshGeodesics_HDRS} )
target_link_libraries( vtkMeshGeodesics MeshGeodesics )
