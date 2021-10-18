PROJECT( MeshGeodesics )

IF(NOT LIBRARY_OUTPUT_PATH)
  SET(LIBRARY_OUTPUT_PATH ${MeshGeodesics_BINARY_DIR}/bin CACHE INTERNAL
    "Single output directory for building all libraries.")
ENDIF(NOT LIBRARY_OUTPUT_PATH)
SET(MeshGeodesics_LIBRARY_DIRS ${LIBRARY_OUTPUT_PATH})
SET(MeshGeodesics_RUNTIME_DIRS ${LIBRARY_OUTPUT_PATH})

IF(NOT EXECUTABLE_OUTPUT_PATH)
  SET(EXECUTABLE_OUTPUT_PATH ${MeshGeodesics_BINARY_DIR}/bin CACHE INTERNAL
    "Single output directory for building all executables.")
ENDIF(NOT EXECUTABLE_OUTPUT_PATH)


SET( ${PROJECT_NAME}_SRCS
     gw_core/GW_Config.cpp
     gw_core/GW_Face.cpp
     gw_core/GW_FaceIterator.cpp
     gw_core/GW_Mesh.cpp
     gw_core/GW_SmartCounter.cpp
     gw_core/GW_Vertex.cpp
     gw_core/GW_VertexIterator.cpp
     gw_geodesic/GW_GeodesicFace.cpp
     gw_geodesic/GW_GeodesicMesh.cpp
     gw_geodesic/GW_GeodesicPath.cpp
     gw_geodesic/GW_GeodesicPoint.cpp
     gw_geodesic/GW_GeodesicVertex.cpp
     gw_geodesic/GW_TriangularInterpolation_Linear.cpp
     gw_geodesic/GW_TriangularInterpolation_Quadratic.cpp
     gw_geodesic/GW_TriangularInterpolation_Cubic.cpp
)

SET( ${PROJECT_NAME}_HDRS
     gw_core/GW_Config.h
     gw_core/GW_Face.h
     gw_core/GW_Mesh.h
     gw_core/GW_FaceIterator.h
     gw_core/GW_Vertex.h
     gw_core/GW_VertexIterator.h
     gw_core/GW_SmartCounter.h
     gw_geodesic/GW_GeodesicFace.h
     gw_geodesic/GW_GeodesicMesh.h
     gw_geodesic/GW_GeodesicPath.h
     gw_geodesic/GW_GeodesicPoint.h
     gw_geodesic/GW_GeodesicVertex.h
     gw_geodesic/GW_TriangularInterpolation_Linear.h
     gw_geodesic/GW_TriangularInterpolation_Quadratic.h
     gw_geodesic/GW_TriangularInterpolation_Cubic.h
)

INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR}/gw_core
                     ${CMAKE_CURRENT_SOURCE_DIR}/gw_geodesic
                     ${CMAKE_CURRENT_BINARY_DIR}/gw_maths )

ADD_LIBRARY( ${PROJECT_NAME} STATIC 
             ${${PROJECT_NAME}_SRCS}
             ${${PROJECT_NAME}_HDRS} )
