import os
import unittest
import logging
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin

#
# SurfaceDistance
#

class SurfaceDistance(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Surface Distance"
    self.parent.categories = ["Surface Models"]
    self.parent.dependencies = []
    self.parent.contributors = ["Ye Han, Jean-Christophe Fillion-Robin, Beatriz Paniagua (Kitware)"]
    self.parent.helpText = """
This module supports computing distance between models either by surface distance or pairwise nodal distance.
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
The development of this module was supported by the NIH National Institute of Biomedical Imaging Bioengineering 
R01EB021391 (Shape Analysis Toolbox for Medical Image Computing Projects), 
with help from Andras Lasso (PerkLab, Queen's University) and Steve Pieper (Isomics, Inc.).
"""

#
# SurfaceDistanceWidget
#

class SurfaceDistanceWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
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
    uiWidget = slicer.util.loadUI(self.resourcePath('UI/SurfaceDistance.ui'))
    self.layout.addWidget(uiWidget)
    self.ui = slicer.util.childWidgetVariables(uiWidget)

    # Set scene in MRML widgets. Make sure that in Qt designer the top-level qMRMLWidget's
    # "mrmlSceneChanged(vtkMRMLScene*)" signal in is connected to each MRML widget's.
    # "setMRMLScene(vtkMRMLScene*)" slot.
    uiWidget.setMRMLScene(slicer.mrmlScene)

    # Create logic class. Logic implements all computations that should be possible to run
    # in batch mode, without a graphical user interface.
    self.logic = SurfaceDistanceLogic()
    self.logic.updateProcessCallback = self.updateProcess

    # Connections

    # These connections ensure that we update parameter node when scene is closed
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose)

    # These connections ensure that whenever user changes some settings on the GUI, that is saved in the MRML scene
    # (in the selected parameter node).

    self.parameterEditWidgets = [
      (self.ui.MRMLNodeComboBox_targetNode, "targetNode"),
      # (self.ui.CheckableNodeComboBox_sourceNodes, "sourceNodes"),  # qMRMLCheckableNodeComboBox is not yet supported
      (self.ui.comboBox_method, "method"),
      (self.ui.checkBox_signedDistance, "signedDistance")
    ]

    slicer.util.addParameterEditWidgetConnections(self.parameterEditWidgets, self.updateParameterNodeFromGUI)
    self.ui.CheckableNodeComboBox_sourceNodes.connect("checkedNodesChanged()", self.updateGUIFromParameterNode)

    # Buttons
    self.ui.applyButton.connect('clicked(bool)', self.onApplyButton)

    # Make sure parameter node is initialized (needed for module reload)
    self.initializeParameterNode()
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
    if not self._parameterNode.GetNodeReference("targetNode"):
      firstModelNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLModelNode")
      if firstModelNode:
          self._parameterNode.SetNodeReferenceID("targetNode", firstModelNode.GetID())

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
    isASD = (self._parameterNode.GetParameter('method') == "Absolute Surface Distance")
    self.ui.checkBox_signedDistance.setEnabled(isASD)
    self.ui.applyButton.setEnabled(self._parameterNode.GetNodeReference("targetNode") and
                                   not self.ui.CheckableNodeComboBox_sourceNodes.noneChecked())

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
      targetNode = self._parameterNode.GetNodeReference("targetNode")
      sourceNodes = self.ui.CheckableNodeComboBox_sourceNodes.checkedNodes()
      self.logic.computeDistance(targetNode, sourceNodes, self._parameterNode)
      # Set source nodes rendering properties and unify scalar range
      scalarRange = [float('inf'), float('-inf')]
      for sourceNode in sourceNodes:
        pointData = sourceNode.GetPolyData().GetPointData()
        pointData.SetActiveScalars("Distance")
        sourceNodeScalarRange = pointData.GetScalars().GetRange()
        scalarRange[0] = min(sourceNodeScalarRange[0], scalarRange[0])
        scalarRange[1] = max(sourceNodeScalarRange[1], scalarRange[1])
      for sourceNode in sourceNodes:
        sourceNode.GetDisplayNode().SetActiveScalar("Distance", vtk.vtkAssignAttribute.POINT_DATA)
        sourceNode.GetDisplayNode().SetAndObserveColorNodeID("vtkMRMLColorTableNodeFileDivergingBlueRed.txt")
        sourceNode.GetDisplayNode().SetScalarRangeFlag(0)
        sourceNode.GetDisplayNode().SetScalarRange(scalarRange)
        sourceNode.GetDisplayNode().SetScalarVisibility(True)
      slicer.app.processEvents()
      self.ui.applyButton.text = "Apply"
    except Exception as e:
      slicer.util.errorDisplay("Failed to compute output model: " + str(e))
      import traceback
      traceback.print_exc()
      sourceNodes.GetModelDisplayNode().VisibilityOn()
    finally:
      slicer.app.resumeRender()
      qt.QApplication.restoreOverrideCursor()


class SurfaceDistanceLogic(ScriptedLoadableModuleLogic):
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
        ("method", "Absolute Surface Distance"),
        ("signedDistance", "true"),
    ]
    for parameterName, defaultValue in defaultValues:
        if not parameterNode.GetParameter(parameterName):
            parameterNode.SetParameter(parameterName, defaultValue)

  def updateProcess(self, message):
    if not self.updateProcessCallback:
        return
    self.updateProcessCallback(message)

  def computeDistance(self, targetNode, sourceNodes, parameterNode):
    """
    Compute surface distance or pairwise distance between points.
    """
    targetShape = targetNode.GetPolyData()
    if parameterNode.GetParameter("method") == "Absolute Surface Distance":
      for sourceNode in sourceNodes:
        sourceShape = sourceNode.GetPolyData()
        distanceFilter = vtk.vtkDistancePolyDataFilter()
        distanceFilter.SetInputData(0, sourceShape)
        distanceFilter.SetInputData(1, targetShape)
        distanceFilter.SetSignedDistance(parameterNode.GetParameter("signedDistance") == "true")
        distanceFilter.ComputeCellCenterDistanceOff()
        distanceFilter.Update()
        sourceNode.SetAndObservePolyData(distanceFilter.GetOutput())
    elif parameterNode.GetParameter("method") == "Points in Correspondence":
      from vtk.util.numpy_support import numpy_to_vtk, vtk_to_numpy
      import numpy as np
      for sourceNode in sourceNodes:
        sourcePoints = vtk_to_numpy(sourceNode.GetPolyData().GetPoints().GetData())
        targetPoints = vtk_to_numpy(targetShape.GetPoints().GetData())
        distance = sourcePoints - targetPoints
        distance = distance.reshape(-1, 3)
        distance = numpy_to_vtk(np.linalg.norm(distance, axis=1))
        distance.SetNumberOfComponents(1)
        distance.SetName("Distance")
        sourceNode.GetPolyData().GetPointData().AddArray(distance)
    else:
      logging.error("Wrong method selection.")


class SurfaceDistanceTest(ScriptedLoadableModuleTest):
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
    self.delayDisplay('Test passed!')
