import os
import unittest
import logging
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin

#
# SurfaceToolbox
#

class SurfaceToolbox(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Surface Toolbox"
    self.parent.categories = ["Surface Models"]
    self.parent.dependencies = []
    self.parent.contributors = [
      "Luca Antiga (Orobix), Ron Kikinis (Brigham and Women's Hospital), Ben Wilson (Kitware)"]
    self.parent.helpText = """
This module supports various cleanup and optimization processes on surface models.
Select the input and output models, and then enable the stages of the pipeline by selecting the buttons.
Stages that include parameters will open up when they are enabled.
Click apply to activate the pipeline and then click the Toggle button to compare the model before and after
 the operation.
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
This module was developed by Luca Antiga, Orobix Srl, with a little help from Steve Pieper, Isomics, Inc.
"""

#
# SurfaceToolboxWidget
#

class SurfaceToolboxWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
  """Uses ScriptedLoadableModuleWidget base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent=None):
    """
    Called when the user opens the module the first time and the widget is initialized.
    """
    ScriptedLoadableModuleWidget.__init__(self, parent)
    VTKObservationMixin.__init__(self)  # needed for parameter node observation
    self.logic = None
    self._parameterNode = None
    self._updatingGUIFromParameterNode = False

  def setup(self):
    """
    Called when the user opens the module the first time and the widget is initialized.
    """
    ScriptedLoadableModuleWidget.setup(self)

    # Load widget from .ui file (created by Qt Designer).
    # Additional widgets can be instantiated manually and added to self.layout.
    uiWidget = slicer.util.loadUI(self.resourcePath('UI/SurfaceToolbox.ui'))
    self.layout.addWidget(uiWidget)
    self.ui = slicer.util.childWidgetVariables(uiWidget)

    # Set scene in MRML widgets. Make sure that in Qt designer the top-level qMRMLWidget's
    # "mrmlSceneChanged(vtkMRMLScene*)" signal in is connected to each MRML widget's.
    # "setMRMLScene(vtkMRMLScene*)" slot.
    uiWidget.setMRMLScene(slicer.mrmlScene)

    # Create logic class. Logic implements all computations that should be possible to run
    # in batch mode, without a graphical user interface.
    self.logic = SurfaceToolboxLogic()
    self.logic.updateProcessCallback = self.updateProcess

    # Connections

    # These connections ensure that we update parameter node when scene is closed
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose)

    # These connections ensure that whenever user changes some settings on the GUI, that is saved in the MRML scene
    # (in the selected parameter node).

    self.parameterEditWidgets = [
      (self.ui.inputModelSelector, "inputModel"),
      (self.ui.outputModelSelector, "outputModel"),
      (self.ui.decimationButton, "decimation"),
      (self.ui.reductionSlider, "decimationReduction"),
      (self.ui.boundaryDeletionCheckBox, "decimationBoundaryDeletion"),
      (self.ui.smoothingButton, "smoothing"),
      (self.ui.smoothingMethodComboBox, "smoothingMethod"),
      (self.ui.laplaceIterationsSlider, "smoothingLaplaceIterations"),
      (self.ui.laplaceRelaxationSlider, "smoothingLaplaceRelaxation"),
      (self.ui.taubinIterationsSlider, "smoothingTaubinIterations"),
      (self.ui.taubinPassBandSlider, "smoothingTaubinPassBand"),
      (self.ui.boundarySmoothingCheckBox, "smoothingBoundarySmoothing"),
      (self.ui.normalsButton, "normals"),
      (self.ui.autoOrientNormalsCheckBox, "normalsAutoOrient"),
      (self.ui.flipNormalsCheckBox, "normalsFlip"),
      (self.ui.splittingCheckBox, "normalsSplitting"),
      (self.ui.featureAngleSlider, "normalsFeatureAngle"),
      (self.ui.mirrorButton, "mirror"),
      (self.ui.mirrorXCheckBox, "mirrorX"),
      (self.ui.mirrorYCheckBox, "mirrorY"),
      (self.ui.mirrorZCheckBox, "mirrorZ"),
      (self.ui.cleanerButton, "cleaner"),
      (self.ui.fillHolesButton, "fillHoles"),
      (self.ui.fillHolesSizeSlider, "fillHolesSize"),
      (self.ui.connectivityButton, "connectivity"),
      (self.ui.scaleMeshButton, "scale"),
      (self.ui.scaleXSlider, "scaleX"),
      (self.ui.scaleYSlider, "scaleY"),
      (self.ui.scaleZSlider, "scaleZ"),
      (self.ui.translateMeshButton, "translate"),
      (self.ui.translationXSlider, "translateX"),
      (self.ui.translationXSlider, "translateY"),
      (self.ui.translationXSlider, "translateZ"),
      (self.ui.relaxButton, "relax"),
      (self.ui.relaxIterationsSlider, "relaxIterations"),
      (self.ui.bordersOutButton, "bordersOut"),
      (self.ui.translateCenterToOriginButton, "translateCenterToOrigin")
    ]

    slicer.util.addParameterEditWidgetConnections(self.parameterEditWidgets, self.updateParameterNodeFromGUI)

    # Buttons
    self.ui.applyButton.connect('clicked(bool)', self.onApplyButton)
    self.ui.toggleModelsButton.connect('clicked()', self.onToggleModels)

    # Make sure parameter node is initialized (needed for module reload)
    self.initializeParameterNode()

    # TODO: connectivity could be
    # - largest connected
    # - threshold connected (discard below point count)
    # - pick a region interactively
    # - turn a multiple connected surface into a model hierarchy

    self.updateGUIFromParameterNode()

  def cleanup(self):
    """
    Called when the application closes and the module widget is destroyed.
    """
    self.removeObservers()

  def enter(self):
    """
    Called each time the user opens this module.
    """
    # Make sure parameter node exists and observed
    self.initializeParameterNode()

  def exit(self):
    """
    Called each time the user opens a different module.
    """
    # Do not react to parameter node changes (GUI wlil be updated when the user enters into the module)
    self.removeObserver(self._parameterNode, vtk.vtkCommand.ModifiedEvent, self.updateGUIFromParameterNode)

  def onSceneStartClose(self, caller, event):
    """
    Called just before the scene is closed.
    """
    # Parameter node will be reset, do not use it anymore
    self.setParameterNode(None)

  def onSceneEndClose(self, caller, event):
    """
    Called just after the scene is closed.
    """
    # If this module is shown while the scene is closed then recreate a new parameter node immediately
    if self.parent.isEntered:
      self.initializeParameterNode()

  def initializeParameterNode(self):
    """
    Ensure parameter node exists and observed.
    """
    # Parameter node stores all user choices in parameter values, node selections, etc.
    # so that when the scene is saved and reloaded, these settings are restored.

    self.setParameterNode(self.logic.getParameterNode())

    # Select default input nodes if nothing is selected yet to save a few clicks for the user
    if not self._parameterNode.GetNodeReference("inputModel"):
      firstModelNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLModelNode")
      if firstModelNode:
        self._parameterNode.SetNodeReferenceID("inputModel", firstModelNode.GetID())

  def setParameterNode(self, inputParameterNode):
    """
    Set and observe parameter node.
    Observation is needed because when the parameter node is changed then the GUI must be updated immediately.
    """

    if inputParameterNode:
      self.logic.setDefaultParameters(inputParameterNode)

    # Unobserve previously selected parameter node and add an observer to the newly selected.
    # Changes of parameter node are observed so that whenever parameters are changed by a script or any other module
    # those are reflected immediately in the GUI.
    if self._parameterNode is not None:
      self.removeObserver(self._parameterNode, vtk.vtkCommand.ModifiedEvent, self.updateGUIFromParameterNode)
    self._parameterNode = inputParameterNode
    if self._parameterNode is not None:
      self.addObserver(self._parameterNode, vtk.vtkCommand.ModifiedEvent, self.updateGUIFromParameterNode)

    # Initial GUI update
    self.updateGUIFromParameterNode()

  def updateGUIFromParameterNode(self, caller=None, event=None):
    """
    This method is called whenever parameter node is changed.
    The module GUI is updated to show the current state of the parameter node.
    """

    if self._parameterNode is None or self._updatingGUIFromParameterNode:
      return

    # Make sure GUI changes do not call updateParameterNodeFromGUI (it could cause infinite loop)
    self._updatingGUIFromParameterNode = True

    # Update node selectors and sliders
    slicer.util.updateParameterEditWidgetsFromNode(self.parameterEditWidgets, self._parameterNode)

    # Update buttons states and tooltips
    isLaplace = (self._parameterNode.GetParameter('smoothingMethod') == "Laplace")
    self.ui.laplaceIterationsLabel.setVisible(isLaplace)
    self.ui.laplaceIterationsSlider.setVisible(isLaplace)
    self.ui.laplaceRelaxationLabel.setVisible(isLaplace)
    self.ui.laplaceRelaxationSlider.setVisible(isLaplace)
    self.ui.taubinIterationsLabel.setVisible(not isLaplace)
    self.ui.taubinIterationsSlider.setVisible(not isLaplace)
    self.ui.taubinPassBandLabel.setVisible(not isLaplace)
    self.ui.taubinPassBandSlider.setVisible(not isLaplace)

    modelsSelected = (self._parameterNode.GetNodeReference("inputModel") and self._parameterNode.GetNodeReference("outputModel"))
    self.ui.toggleModelsButton.enabled = modelsSelected
    self.ui.applyButton.enabled = modelsSelected

    # All the GUI updates are done
    self._updatingGUIFromParameterNode = False

  def updateParameterNodeFromGUI(self, caller=None, event=None):
    """
    This method is called when the user makes any change in the GUI.
    The changes are saved into the parameter node (so that they are restored when the scene is saved and loaded).
    """
    if self._parameterNode is None or self._updatingGUIFromParameterNode:
      return
    wasModified = self._parameterNode.StartModify()  # Modify all properties in a single batch
    slicer.util.updateNodeFromParameterEditWidgets(self.parameterEditWidgets, self._parameterNode)
    self._parameterNode.EndModify(wasModified)

  def updateProcess(self, value):
    """Display changing process value"""
    self.ui.applyButton.text = value
    self.ui.applyButton.repaint()

  def onApplyButton(self):
    """
    Run processing when user clicks "Apply" button.
    """
    slicer.app.pauseRender()
    qt.QApplication.setOverrideCursor(qt.Qt.WaitCursor)
    try:
      inputModelNode = self._parameterNode.GetNodeReference("inputModel")
      outputModelNode = self._parameterNode.GetNodeReference("outputModel")
      self.logic.applyFilters(self._parameterNode)
      slicer.app.processEvents()
      inputModelNode.GetModelDisplayNode().VisibilityOff()
      outputModelNode.GetModelDisplayNode().VisibilityOn()
      #slicer.updateGUIFromParameterNode()
      self.ui.applyButton.text = "Apply"
    except Exception as e:
      slicer.util.errorDisplay("Failed to compute output model: "+str(e))
      import traceback
      traceback.print_exc()
      inputModelNode.GetModelDisplayNode().VisibilityOn()
      outputModelNode.GetModelDisplayNode().VisibilityOff()
    finally:
      slicer.app.resumeRender()
      qt.QApplication.restoreOverrideCursor()

  def onToggleModels(self):
    inputModelNode = self._parameterNode.GetNodeReference("inputModel")
    outputModelNode = self._parameterNode.GetNodeReference("outputModel")
    if inputModelNode.GetModelDisplayNode().GetVisibility():
      inputModelNode.GetModelDisplayNode().VisibilityOff()
      outputModelNode.GetModelDisplayNode().VisibilityOn()
      self.ui.toggleModelsButton.text = "Toggle Models (Output)"
    else:
      inputModelNode.GetModelDisplayNode().VisibilityOn()
      outputModelNode.GetModelDisplayNode().VisibilityOff()
      self.ui.toggleModelsButton.text = "Toggle Models (Input)"

#
# SurfaceToolboxLogic
#

class SurfaceToolboxLogic(ScriptedLoadableModuleLogic):
  """This class should implement all the actual
  computation done by your module.  The interface
  should be such that other python code can import
  this class and make use of the functionality without
  requiring an instance of the Widget.
  Uses ScriptedLoadableModuleLogic base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self):
    """
    Called when the logic class is instantiated. Can be used for initializing member variables.
    """
    ScriptedLoadableModuleLogic.__init__(self)
    self.updateProcessCallback = None

  def setDefaultParameters(self, parameterNode):
    """
    Initialize parameter node with default settings.
    """
    defaultValues = [
      ("decimation", "false"),
      ("decimationReduction", "0.8"),
      ("decimationBoundaryDeletion", "true"),
      ("smoothing", "false"),
      ("smoothingMethod", "Laplace"),
      ("smoothingLaplaceIterations", "100"),
      ("smoothingLaplaceRelaxation", "0.5"),
      ("smoothingTaubinIterations", "30"),
      ("smoothingTaubinPassBand", "0.1"),
      ("smoothingBoundarySmoothing", "true"),
      ("normals", "false"),
      ("normalsAutoOrient", "false"),
      ("normalsFlip", "false"),
      ("normalsSplitting", "false"),
      ("normalsFeatureAngle", "30.0"),
      ("mirror", "false"),
      ("mirrorX", "false"),
      ("mirrorY", "false"),
      ("mirrorZ", "false"),
      ("cleaner", "false"),
      ("fillHoles", "false"),
      ("fillHolesSize", "1000.0"),
      ("connectivity", "false"),
      ("scale", "false"),
      ("scaleX", "0.5"),
      ("scaleY", "0.5"),
      ("scaleZ", "0.5"),
      ("translate", "false"),
      ("translateX", "0.0"),
      ("translateY", "0.0"),
      ("translateZ", "0.0"),
      ("relax", "false"),
      ("relaxIterations", "5"),
      ("bordersOut", "false"),
      ("translateCenterToOrigin", "false")
    ]
    for parameterName, defaultValue in defaultValues:
      if not parameterNode.GetParameter(parameterName):
        parameterNode.SetParameter(parameterName, defaultValue)

  def updateProcess(self, message):
    if not self.updateProcessCallback:
      return
    self.updateProcessCallback(message)

  def runCLI(self, module, parameters):
    cliNode = slicer.cli.runSync(module, None, parameters)
    slicer.mrmlScene.RemoveNode(cliNode)

  def applyFilters(self, parameterNode):
    import time
    startTime = time.time()
    logging.info('Processing started')

    inputModel = parameterNode.GetNodeReference("inputModel")

    outputModel = parameterNode.GetNodeReference("outputModel")
    if outputModel.GetPolyData() is None:
      outputModel.SetAndObserveMesh(vtk.vtkPolyData())
    outputModel.GetPolyData().DeepCopy(inputModel.GetPolyData())

    outputModel.CreateDefaultDisplayNodes()
    outputModel.AddDefaultStorageNode()

    if parameterNode.GetParameter("decimation") == "true":
      self.updateProcess("Decimation...")
      decimateBoundary = parameterNode.GetParameter("decimationBoundary") == "true"
      parameters = {
        "inputModel": outputModel,
        "outputModel": outputModel,
        "reductionFactor": float(parameterNode.GetParameter("decimationReduction"))
        }
      if decimateBoundary:
        # use default FastQuadric decimation method if boundary can be decimated
        parameters["method"] = "FastQuadric"
      else:
        parameters["method"] = "DecimatePro"
        parameters["boundaryDeletion"] = False
      self.runCLI(slicer.modules.decimation, parameters)

    if parameterNode.GetParameter("smoothing") == "true":
      self.updateProcess("Smoothing...")
      if parameterNode.GetParameter("smoothingMethod") == "Laplace":
        smoothing = vtk.vtkSmoothPolyDataFilter()
        smoothing.SetBoundarySmoothing(parameterNode.GetParameter("smoothingBoundarySmoothing") == "true")
        smoothing.SetNumberOfIterations(int(float(parameterNode.GetParameter("smoothingLaplaceIterations"))))
        smoothing.SetRelaxationFactor(float(parameterNode.GetParameter("smoothingLaplaceRelaxation")))
      else:  # "Taubin"
        smoothing = vtk.vtkWindowedSincPolyDataFilter()
        smoothing.SetBoundarySmoothing(parameterNode.GetParameter("smoothingBoundarySmoothing") == "true")
        smoothing.SetNumberOfIterations(int(float(parameterNode.GetParameter("smoothingTaubinIterations"))))
        smoothing.SetPassBand(float(parameterNode.GetParameter("smoothingTaubinPassBand")))
      smoothing.SetInputData(outputModel.GetPolyData())
      smoothing.Update()
      outputModel.SetAndObservePolyData(smoothing.GetOutput())

    if parameterNode.GetParameter("normals") == "true":
      self.updateProcess("Normals...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel,
        "orient": parameterNode.GetParameter("normalsOrient") == "true",
        "flip": parameterNode.GetParameter("normalsFlip") == "true",
        "splitting": parameterNode.GetParameter("normalsSplitting") == "true",
        "angle": float(parameterNode.GetParameter("normalsFeatureAngle"))}
      self.runCLI(slicer.modules.normals, parameters)

    if parameterNode.GetParameter("mirror") == "true":
      self.updateProcess("Mirror...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel,
        "xAxis": parameterNode.GetParameter("mirrorX") == "true",
        "yAxis": parameterNode.GetParameter("mirrorY") == "true",
        "zAxis": parameterNode.GetParameter("mirrorZ") == "true"
        }
      self.runCLI(slicer.modules.mirror, parameters)

    if parameterNode.GetParameter("cleaner") == "true":
      self.updateProcess("Cleaner...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel
        }
      self.runCLI(slicer.modules.cleaner, parameters)

    if parameterNode.GetParameter("fillHoles") == "true":
      self.updateProcess("Fill Holes...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel,
        "holes": float(parameterNode.GetParameter("fillHolesSize"))
        }
      self.runCLI(slicer.modules.fillholes, parameters)

    if parameterNode.GetParameter("connectivity") == "true":
      self.updateProcess("Connectivity...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel
        }
      self.runCLI(slicer.modules.connectivity, parameters)

    if parameterNode.GetParameter("scale") == "true":
      self.updateProcess("Scale...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel,
        "dimX": float(parameterNode.GetParameter("scaleX")),
        "dimY": float(parameterNode.GetParameter("scaleY")),
        "dimZ": float(parameterNode.GetParameter("scaleZ"))
        }
      self.runCLI(slicer.modules.scalemesh, parameters)

    if parameterNode.GetParameter("translate") == "true":
      self.updateProcess("Translating...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel,
        "dimX": float(parameterNode.GetParameter("translateX")),
        "dimY": float(parameterNode.GetParameter("translateY")),
        "dimZ": float(parameterNode.GetParameter("translateZ"))
        }
      self.runCLI(slicer.modules.translatemesh, parameters)

    if parameterNode.GetParameter("relax") == "true":
      self.updateProcess("Relaxing...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel,
        "Iterations": int(float(parameterNode.GetParameter("relaxIterations")))
        }
      self.runCLI(slicer.modules.relaxpolygons, parameters)

    if parameterNode.GetParameter("border") == "true":
      self.updateProcess("Changing Borders...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel
      }
      self.runCLI(slicer.modules.bordersout, parameters)

    if parameterNode.GetParameter("translateCenterToOrigin") == "true":
      self.updateProcess("Moving Origin...")
      parameters = {
        "inputVolume": outputModel,
        "outputVolume": outputModel
      }
      self.runCLI(slicer.modules.mc2origin, parameters)

    self.updateProcess("Done.")

    stopTime = time.time()
    logging.info('Processing completed in {0:.2f} seconds'.format(stopTime-startTime))

#
# SurfaceToolboxTest
#

class SurfaceToolboxTest(ScriptedLoadableModuleTest):
  """
  This is the test case for your scripted module.
  Uses ScriptedLoadableModuleTest base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
    self.test_AllProcessing()

  def test_AllProcessing(self):
    """ Ideally you should have several levels of tests.  At the lowest level
    tests should exercise the functionality of the logic with different inputs
    (both valid and invalid).  At higher levels your tests should emulate the
    way the user would interact with your code and confirm that it still works
    the way you intended.
    One of the most important features of the tests is that it should alert other
    developers when their changes will have an impact on the behavior of your
    module.  For example, if a developer removes a feature that you depend on,
    your test should break so they know that the feature is needed.
    """

    self.delayDisplay("Starting the test")
    #
    # first, get some data
    #
    import SampleData
    modelNode = SampleData.downloadFromURL(
      nodeNames='cow',
      fileNames='cow.vtp',
      loadFileTypes='ModelFile',
      uris='https://github.com/Slicer/SlicerTestingData/releases/download/SHA256/d5aa4901d186902f90e17bf3b5917541cb6cb8cf223bfeea736631df4c047652',
      checksums='SHA256:d5aa4901d186902f90e17bf3b5917541cb6cb8cf223bfeea736631df4c047652')[0]
    self.delayDisplay('Finished with download and loading')

    logic = SurfaceToolboxLogic()
    self.assertIsNotNone(logic)

    parameterNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLScriptedModuleNode")
    logic.setDefaultParameters(parameterNode)

    parameterNode.SetNodeReferenceID("inputModel", modelNode.GetID())

    outputModelNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLModelNode", "output")
    parameterNode.SetNodeReferenceID("outputModel", outputModelNode.GetID())

    parameterNode.SetParameter("decimation", "true")
    parameterNode.SetParameter("smoothing", "true")
    parameterNode.SetParameter("normals", "true")
    parameterNode.SetParameter("mirror", "true")
    parameterNode.SetParameter("mirrorX", "true")
    parameterNode.SetParameter("cleaner", "true")
    parameterNode.SetParameter("fillHoles", "true")
    parameterNode.SetParameter("connectivity", "true")
    parameterNode.SetParameter("scale", "true")
    parameterNode.SetParameter("translate", "true")
    parameterNode.SetParameter("translateX", "5.12")
    parameterNode.SetParameter("relax", "true")
    parameterNode.SetParameter("bordersOut", "true")
    parameterNode.SetParameter("translateCenterToOrigin", "true")

    self.delayDisplay('Module selected, input and output configured')

    logic.applyFilters(parameterNode)

    self.delayDisplay('Test passed!')
