project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICER_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkAddon_INCLUDE_DIRS}
  )

set(${KIT}_SRCS
  vtkImplicitPolyDataPointDistance.cxx
  vtkImplicitPolyDataPointDistance.h
  vtkSlicer${MODULE_NAME}AppendRule.cxx
  vtkSlicer${MODULE_NAME}AppendRule.h
  vtkSlicer${MODULE_NAME}BoundaryCutRule.cxx
  vtkSlicer${MODULE_NAME}BoundaryCutRule.h
  vtkSlicer${MODULE_NAME}CurveCutRule.cxx
  vtkSlicer${MODULE_NAME}CurveCutRule.h
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  vtkSlicer${MODULE_NAME}MirrorRule.cxx
  vtkSlicer${MODULE_NAME}MirrorRule.h
  vtkSlicer${MODULE_NAME}PlaneCutRule.cxx
  vtkSlicer${MODULE_NAME}PlaneCutRule.h
  vtkSlicer${MODULE_NAME}Rule.cxx
  vtkSlicer${MODULE_NAME}Rule.h
  vtkSlicer${MODULE_NAME}RuleFactory.cxx
  vtkSlicer${MODULE_NAME}RuleFactory.h
  )

set(${KIT}_TARGET_LIBRARIES
  vtkSlicer${MODULE_NAME}ModuleMRML
  vtkAddon
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
