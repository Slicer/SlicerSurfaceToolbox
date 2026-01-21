cmake_minimum_required(VERSION 3.13.4)

project(SurfaceToolbox)

list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS
  ${CMAKE_CURRENT_LIST_DIR}/DynamicModeler/Resources/UI
  ${CMAKE_CURRENT_LIST_DIR}/SurfaceToolbox/Resources/UI
)

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(Decimation)
add_subdirectory(SurfaceToolbox)
add_subdirectory(DynamicModeler)
add_subdirectory(MeshAlignment)

## NEXT_MODULE

#-----------------------------------------------------------------------------
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
