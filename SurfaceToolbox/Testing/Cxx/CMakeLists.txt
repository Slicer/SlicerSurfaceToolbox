set(KIT vtkSlicer${MODULE_NAME}ModuleLogic)

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  vtkACVDRemeshFilterTest.cxx
  )

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRS} ${MRMLCore_INCLUDE_DIRS}
  TARGET_LIBRARIES vtkAddon
  WITH_VTK_DEBUG_LEAKS_CHECK
  WITH_VTK_ERROR_OUTPUT_CHECK
  )

#-----------------------------------------------------------------------------
simple_test(vtkACVDRemeshFilterTest)
