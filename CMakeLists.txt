cmake_minimum_required(VERSION 3.13.4)

project(SurfaceToolbox)

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(BordersOut)
add_subdirectory(Cleaner)
add_subdirectory(Connectivity)
add_subdirectory(Decimation)
add_subdirectory(FillHoles)
add_subdirectory(MC2Origin)
add_subdirectory(Mirror)
add_subdirectory(Normals)
add_subdirectory(relaxPolygons)
add_subdirectory(Smoothing)
add_subdirectory(SurfaceToolbox)
add_subdirectory(scaleMesh)
add_subdirectory(translateMesh)
add_subdirectory(ParametricSurfaceEditor)



## NEXT_MODULE

#-----------------------------------------------------------------------------
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
