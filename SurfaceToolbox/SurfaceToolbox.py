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
      (self.ui.translateToOriginCheckBox, "translateToOrigin"),
      (self.ui.translationXSlider, "translateX"),
      (self.ui.translationYSlider, "translateY"),
      (self.ui.translationZSlider, "translateZ"),
      (self.ui.extractEdgesButton, "extractEdges"),
      (self.ui.extractEdgesBoundaryCheckBox, "extractEdgesBoundary"),
      (self.ui.extractEdgesFeatureCheckBox, "extractEdgesFeature"),
      (self.ui.extractEdgesFeatureAngleSlider, "extractEdgesFeatureAngle"),
      (self.ui.extractEdgesNonManifoldCheckBox, "extractEdgesNonManifold"),
      (self.ui.extractEdgesManifoldCheckBox, "extractEdgesManifold"),
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
      ("smoothingMethod", "Taubin"),
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
      ("translateToOriginCheckBox", "false"),
      ("translateX", "0.0"),
      ("translateY", "0.0"),
      ("translateZ", "0.0"),
      ("extractEdges", "false"),
      ("extractEdgesBoundary", "true"),
      ("extractEdgesFeature", "true"),
      ("extractEdgesFeatureAngle", "20"),
      ("extractEdgesNonManifold", "false"),
      ("extractEdgesManifold", "false"),
    ]
    for parameterName, defaultValue in defaultValues:
      if not parameterNode.GetParameter(parameterName):
        parameterNode.SetParameter(parameterName, defaultValue)

  def updateProcess(self, message):
    if not self.updateProcessCallback:
      return
    self.updateProcessCallback(message)

  @staticmethod
  def decimate(inputModel, outputModel, reductionFactor=0.8, decimateBoundary=True, lossless=False, aggressiveness=7.0):
    """Perform a topology-preserving reduction of surface triangles. FastMesh method uses Sven Forstmann's method
    (https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification).

    :param reductionFactor: Target reduction factor during decimation. Ratio of triangles that are requested to
      be eliminated. 0.8 means that the mesh size is requested to be reduced by 80%.
    :param decimateBoundary: If enabled then 'FastQuadric' method is used (it provides more even element sizes but cannot
      be forced to preserve boundary), otherwise 'DecimatePro' method is used (that can preserve boundary edges but tend
      to create more ill-shaped triangles).
    :param lossless: Lossless remeshing for FastQuadric method. The flag has no effect if other method is used.
    :param aggressiveness: Balances between accuracy and computation time for FastQuadric method (default = 7.0). The flag has no effect if other method is used.
    """
    parameters = {
      "inputModel": inputModel,
      "outputModel": outputModel,
      "reductionFactor": reductionFactor,
      "method": "FastQuadric" if decimateBoundary else "DecimatePro",
      "boundaryDeletion": decimateBoundary
      }
    cliNode = slicer.cli.runSync(slicer.modules.decimation, None, parameters)
    slicer.mrmlScene.RemoveNode(cliNode)

  @staticmethod
  def smooth(inputModel, outputModel, method='Taubin', iterations=30, laplaceRelaxationFactor=0.5, taubinPassBand=0.1, boundarySmoothing=True):
    """Smoothes surface model using a Laplacian filter or Taubin's non-shrinking algorithm.
    """
    if method == "Laplace":
      smoothing = vtk.vtkSmoothPolyDataFilter()
      smoothing.SetRelaxationFactor(laplaceRelaxationFactor)
    else:  # "Taubin"
      smoothing = vtk.vtkWindowedSincPolyDataFilter()
      smoothing.SetPassBand(taubinPassBand)
    smoothing.SetBoundarySmoothing(boundarySmoothing)
    smoothing.SetNumberOfIterations(iterations)
    smoothing.SetInputData(inputModel.GetPolyData())
    smoothing.Update()
    outputModel.SetAndObservePolyData(smoothing.GetOutput())

  @staticmethod
  def fillHoles(inputModel, outputModel, maximumHoleSize=1000.0):
    """Fills up a hole in a open mesh.
    """
    fill = vtk.vtkFillHolesFilter()
    fill.SetInputData(inputModel.GetPolyData())
    fill.SetHoleSize(maximumHoleSize)

    # Need to auto-orient normals, otherwise holes could appear to be unfilled when
    # only front-facing elements are chosen to be visible.
    normals = vtk.vtkPolyDataNormals()
    normals.SetInputConnection(fill.GetOutputPort())
    normals.SetAutoOrientNormals(True)
    normals.Update()
    outputModel.SetAndObservePolyData(normals.GetOutput())

  @staticmethod
  def transform(inputModel, outputModel, scaleX=1.0, scaleY=1.0, scaleZ=1.0, translateX=0.0, translateY=0.0, translateZ=0.0):
    """Mesh relaxation based on vtkWindowedSincPolyDataFilter.
    Scale of 1.0 means original size, >1.0 means magnification.
    """
    transform = vtk.vtkTransform()
    transform.Translate(translateX, translateY, translateZ)
    transform.Scale(scaleX, scaleY, scaleZ)
    transformFilter = vtk.vtkTransformFilter()
    transformFilter.SetInputData(inputModel.GetPolyData())
    transformFilter.SetTransform(transform)

    if transform.GetMatrix().Determinant() >= 0.0:
      transformFilter.Update()
      outputModel.SetAndObservePolyData(transformFilter.GetOutput())
    else:
      # The mesh is turned inside out, reverse the mesh cells to keep them facing outside
      reverse = vtk.vtkReverseSense()
      reverse.SetInputConnection(transformFilter.GetOutputPort())
      reverse.Update()
      outputModel.SetAndObservePolyData(reverse.GetOutput())

  @staticmethod
  def translateCenterToOrigin(inputModel, outputModel):
    """Translate center of the mesh bounding box to the origin.
    """
    bounds = inputModel.GetMesh().GetBounds()
    centerPosition = [(bounds[1]+bounds[0])/2.0, (bounds[3]+bounds[2])/2.0, (bounds[5]+bounds[4])/2.0]
    SurfaceToolboxLogic.transform(inputModel, outputModel, translateX=-centerPosition[0], translateY=-centerPosition[1], translateZ=-centerPosition[2])

  @staticmethod
  def clean(inputModel, outputModel):
    """Merge coincident points, remove unused points (i.e. not used by any cell), treatment of degenerate cells.
    """
    cleaner = vtk.vtkCleanPolyData()
    cleaner.SetInputData(inputModel.GetPolyData())
    cleaner.Update()
    outputModel.SetAndObservePolyData(cleaner.GetOutput())

  @staticmethod
  def extractBoundaryEdges(inputModel, outputModel, boundary=False, feature=False, nonManifold=False, manifold=False, featureAngle=20):
    """Extract edges of a model.
    """
    boundaryEdges = vtk.vtkFeatureEdges()
    boundaryEdges.SetInputData(inputModel.GetPolyData())
    boundaryEdges.ExtractAllEdgeTypesOff()
    boundaryEdges.SetBoundaryEdges(boundary)
    boundaryEdges.SetFeatureEdges(feature)
    if feature:
      boundaryEdges.SetFeatureAngle(featureAngle)
    boundaryEdges.SetNonManifoldEdges(nonManifold)
    boundaryEdges.SetManifoldEdges(manifold)
    boundaryEdges.Update()
    outputModel.SetAndObservePolyData(boundaryEdges.GetOutput())

  @staticmethod
  def computeNormals(inputModel, outputModel, autoOrient=False, flip=False, split=False, splitAngle=30.0):
    """Generate surface normals for geometry algorithms or for improving visualization.
    :param splitAngle: Normals will be split only along those edges where angle is larger than this value.
    """
    normals = vtk.vtkPolyDataNormals()
    normals.SetInputData(inputModel.GetPolyData())
    normals.SetAutoOrientNormals(autoOrient)
    normals.SetFlipNormals(flip)
    normals.SetSplitting(split)
    if split:
      # only applicable if splitting is enabled
      normals.SetFeatureAngle(splitAngle)
    normals.Update()
    outputModel.SetAndObservePolyData(normals.GetOutput())

  @staticmethod
  def extractLargestConnectedComponent(inputModel, outputModel):
    """Extract the largest connected portion of a surface model.
    """
    connect = vtk.vtkPolyDataConnectivityFilter()
    connect.SetInputData(inputModel.GetPolyData())
    connect.SetExtractionModeToLargestRegion()
    connect.Update()
    outputModel.SetAndObservePolyData(connect.GetOutput())

  def applyFilters(self, parameterNode):
    import time
    startTime = time.time()
    logging.info('Processing started')

    inputModel = parameterNode.GetNodeReference("inputModel")
    outputModel = parameterNode.GetNodeReference("outputModel")

    if outputModel != inputModel:
      if outputModel.GetPolyData() is None:
        outputModel.SetAndObserveMesh(vtk.vtkPolyData())
      outputModel.GetPolyData().DeepCopy(inputModel.GetPolyData())

    outputModel.CreateDefaultDisplayNodes()
    outputModel.AddDefaultStorageNode()

    if parameterNode.GetParameter("cleaner") == "true":
      self.updateProcess("Clean...")
      SurfaceToolboxLogic.clean(outputModel, outputModel)

    if parameterNode.GetParameter("decimation") == "true":
      self.updateProcess("Decimation...")
      SurfaceToolboxLogic.decimate(outputModel, outputModel,
        reductionFactor=float(parameterNode.GetParameter("decimationReduction")),
        decimateBoundary=parameterNode.GetParameter("decimationBoundaryDeletion") == "true")

    if parameterNode.GetParameter("smoothing") == "true":
      self.updateProcess("Smoothing...")
      method = parameterNode.GetParameter("smoothingMethod")
      SurfaceToolboxLogic.smooth(outputModel, outputModel,
        method=parameterNode.GetParameter("smoothingMethod"),
        iterations=int(float(parameterNode.GetParameter("smoothingLaplaceIterations" if method=='Laplace' else "smoothingTaubinIterations"))),
        laplaceRelaxationFactor=float(parameterNode.GetParameter("smoothingLaplaceRelaxation")),
        taubinPassBand=float(parameterNode.GetParameter("smoothingTaubinPassBand")),
        boundarySmoothing=parameterNode.GetParameter("smoothingBoundarySmoothing") == "true")

    if parameterNode.GetParameter("fillHoles") == "true":
      self.updateProcess("Fill Holes...")
      SurfaceToolboxLogic.fillHoles(outputModel, outputModel,
        float(parameterNode.GetParameter("fillHolesSize")))

    if parameterNode.GetParameter("normals") == "true":
      self.updateProcess("Normals...")
      SurfaceToolboxLogic.computeNormals(outputModel, outputModel,
        autoOrient = parameterNode.GetParameter("normalsOrient") == "true",
        flip=parameterNode.GetParameter("normalsFlip") == "true",
        split=parameterNode.GetParameter("normalsSplitting") == "true",
        splitAngle=float(parameterNode.GetParameter("normalsFeatureAngle")))

    if parameterNode.GetParameter("mirror") == "true":
      self.updateProcess("Mirror...")
      SurfaceToolboxLogic.transform(outputModel, outputModel,
        scaleX = -1.0 if parameterNode.GetParameter("mirrorX") == "true" else 1.0,
        scaleY = -1.0 if parameterNode.GetParameter("mirrorY") == "true" else 1.0,
        scaleZ = -1.0 if parameterNode.GetParameter("mirrorZ") == "true" else 1.0)

    if parameterNode.GetParameter("scale") == "true":
      self.updateProcess("Scale...")
      SurfaceToolboxLogic.transform(outputModel, outputModel,
        scaleX = float(parameterNode.GetParameter("scaleX")),
        scaleY = float(parameterNode.GetParameter("scaleY")),
        scaleZ = float(parameterNode.GetParameter("scaleZ")))

    if parameterNode.GetParameter("translate") == "true":
      self.updateProcess("Translating...")
      if parameterNode.GetParameter("translateToOrigin") == "true":
        SurfaceToolboxLogic.translateCenterToOrigin(outputModel, outputModel)
      SurfaceToolboxLogic.transform(outputModel, outputModel,
        translateX = float(parameterNode.GetParameter("translateX")),
        translateY = float(parameterNode.GetParameter("translateY")),
        translateZ = float(parameterNode.GetParameter("translateZ")))

    if parameterNode.GetParameter("extractEdges") == "true":
      self.updateProcess("Extracting boundary edges...")
      SurfaceToolboxLogic.extractBoundaryEdges(outputModel, outputModel,
        boundary = parameterNode.GetParameter("extractEdgesBoundary") == "true",
        feature = parameterNode.GetParameter("extractEdgesFeature") == "true",
        nonManifold = parameterNode.GetParameter("extractEdgesNonManifold") == "true",
        manifold = parameterNode.GetParameter("extractEdgesManifold") == "true",
        featureAngle = float(parameterNode.GetParameter("extractEdgesFeatureAngle")))

    if parameterNode.GetParameter("connectivity") == "true":
      self.updateProcess("Extract largest connected component...")
      SurfaceToolboxLogic.extractLargestConnectedComponent(outputModel, outputModel)

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
    parameterNode.SetParameter("extractEdges", "true")
    parameterNode.SetParameter("translateToOrigin", "true")

    self.delayDisplay('Module selected, input and output configured')

    logic.applyFilters(parameterNode)

    self.delayDisplay('Test passed!')
