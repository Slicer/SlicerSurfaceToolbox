import os
import unittest
import logging
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin

#
# MeshDataImprinter
#

class MeshDataImprinter(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Mesh Data Imprinter"
    self.parent.categories = ["Surface Models"]
    self.parent.dependencies = []
    self.parent.contributors = ["Ye Han, Jean-Christophe Fillion-Robin, Beatriz Paniagua (Kitware)"]
    self.parent.helpText = """
This module supports transferring data to a target surface model from a source image, a file or another source surface 
model via interpolation or direct data import. Source surface model or image can be selected from the MRML scene. 
Source file should be in .csv format with number of rows (not counting in header) matching number of points in the 
target model. 
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
The development of this module was supported by the NIH National Institute of Biomedical Imaging Bioengineering 
R01EB021391 (Shape Analysis Toolbox for Medical Image Computing Projects), 
with help from Andras Lasso (PerkLab, Queen's University) and Steve Pieper (Isomics, Inc.).
"""

#
# MeshDataImprinterWidget
#

class MeshDataImprinterWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
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
    uiWidget = slicer.util.loadUI(self.resourcePath('UI/MeshDataImprinter.ui'))
    self.layout.addWidget(uiWidget)
    self.ui = slicer.util.childWidgetVariables(uiWidget)

    # Set scene in MRML widgets. Make sure that in Qt designer the top-level qMRMLWidget's
    # "mrmlSceneChanged(vtkMRMLScene*)" signal in is connected to each MRML widget's.
    # "setMRMLScene(vtkMRMLScene*)" slot.
    uiWidget.setMRMLScene(slicer.mrmlScene)

    # Create logic class. Logic implements all computations that should be possible to run
    # in batch mode, without a graphical user interface.
    self.logic = MeshDataImprinterLogic()
    self.logic.updateProcessCallback = self.updateProcess

    # Connections

    # These connections ensure that we update parameter node when scene is closed
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose)

    # These connections ensure that whenever user changes some settings on the GUI, that is saved in the MRML scene
    # (in the selected parameter node).
    self.parameterEditWidgets = [
      (self.ui.comboBox_type, "type"),
      (self.ui.MRMLNodeComboBox_sourceModel, "sourceModel"),
      (self.ui.comboBox_meshMethod, "meshMethod"),
      (self.ui.doubleSpinBox_kernelRadius, "kernelRadius"),
      (self.ui.doubleSpinBox_kernelSharpness, "kernelSharpness"),
      (self.ui.MRMLNodeComboBox_sourceImage, "sourceImage"),
      (self.ui.comboBox_imageMethod, "imageMethod"),
      # (self.ui.PathLineEdit_sourceFile, "sourceFile"),  # ctkPathlineEdit not yet added in util.py for parameter node
      (self.ui.MRMLNodeComboBox_targetNode, "targetNode")
    ]

    slicer.util.addParameterEditWidgetConnections(self.parameterEditWidgets, self.updateParameterNodeFromGUI)
    self.ui.PathLineEdit_sourceFile.connect("currentPathChanged(QString)", self.updateGUIFromParameterNode)

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
    if self._parameterNode.GetParameter("type") == "Mesh to Mesh":
      self.ui.groupBox_meshToMesh.enabled = True
      self.ui.groupBox_imageToMesh.enabled = False
      self.ui.groupBox_fileToMesh.enabled = False
      if self._parameterNode.GetParameter("meshMethod") == "Gaussian":
        self.ui.doubleSpinBox_kernelRadius.enabled = True
        self.ui.doubleSpinBox_kernelSharpness.enabled = True
      else:  # Closest Point
        self.ui.doubleSpinBox_kernelRadius.enabled = False
        self.ui.doubleSpinBox_kernelSharpness.enabled = False
      self.ui.applyButton.setEnabled(self._parameterNode.GetNodeReference("sourceModel") and
                                     self._parameterNode.GetNodeReference("targetNode"))
    elif self._parameterNode.GetParameter("type") == "Image to Mesh":
      self.ui.groupBox_meshToMesh.enabled = False
      self.ui.groupBox_imageToMesh.enabled = True
      self.ui.groupBox_fileToMesh.enabled = False
      self.ui.applyButton.setEnabled(self._parameterNode.GetNodeReference("sourceImage") and
                                     self._parameterNode.GetNodeReference("targetNode"))
    else:  # File to Mesh
      self.ui.groupBox_meshToMesh.enabled = False
      self.ui.groupBox_imageToMesh.enabled = False
      self.ui.groupBox_fileToMesh.enabled = True
      self.ui.applyButton.setEnabled((self.ui.PathLineEdit_sourceFile.currentPath != "") and
                                     self._parameterNode.GetNodeReference("targetNode"))

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
      self.logic.imprintData(self._parameterNode, self.ui.PathLineEdit_sourceFile.currentPath)
      targetNode = self._parameterNode.GetNodeReference("targetNode")
      if self._parameterNode.GetParameter("type") == "Mesh to Mesh":
        sourceDisplayNode = self._parameterNode.GetNodeReference("sourceModel").GetDisplayNode()
        targetDisplayNode = targetNode.GetDisplayNode()
        targetDisplayNode.SetActiveScalar(sourceDisplayNode.GetActiveScalarName(), vtk.vtkAssignAttribute.POINT_DATA)
        targetDisplayNode.SetScalarRange(sourceDisplayNode.GetScalarRange())
        targetDisplayNode.SetAndObserveColorNodeID(sourceDisplayNode.GetColorNodeID())
      else:
        targetNode.GetDisplayNode().SetActiveScalar(targetNode.GetPolyData().GetPointData().GetScalars().GetName(),
                                                    vtk.vtkAssignAttribute.POINT_DATA)
        targetNode.GetDisplayNode().SetAndObserveColorNodeID("vtkMRMLColorTableNodeRainbow")
      targetNode.GetDisplayNode().SetScalarVisibility(True)
      slicer.app.processEvents()
      self.ui.applyButton.text = "Apply"
    except Exception as e:
      slicer.util.errorDisplay("Failed to imprint mesh data: " + str(e))
      import traceback
      traceback.print_exc()
    finally:
      slicer.app.resumeRender()
      qt.QApplication.restoreOverrideCursor()


class MeshDataImprinterLogic(ScriptedLoadableModuleLogic):
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
      ("type", "Mesh to Mesh"),
      ("meshMethod", "Closest Point"),
      ("kernelRadius", "1.0"),
      ("kernelSharpness", "2.0"),
      ("imageMethod", "Linear")
    ]
    for parameterName, defaultValue in defaultValues:
        if not parameterNode.GetParameter(parameterName):
            parameterNode.SetParameter(parameterName, defaultValue)

  def updateProcess(self, message):
    if not self.updateProcessCallback:
        return
    self.updateProcessCallback(message)

  def imprintData(self, parameterNode, filePath):
    """
    Imprint point data from source node to the target node.
    """
    targetNode = parameterNode.GetNodeReference("targetNode")
    if parameterNode.GetParameter("type") == "Mesh to Mesh":
      sourceNode = parameterNode.GetNodeReference("sourceModel")
      interpolator = vtk.vtkPointInterpolator()
      if parameterNode.GetParameter("meshMethod") == "Closest Point":
        interpolator.SetKernel(vtk.vtkVoronoiKernel())
      else:  # Gaussian
        gaussianKernel = vtk.vtkGaussianKernel()
        gaussianKernel.SetRadius(float(parameterNode.GetParameter("kernelRadius")))
        gaussianKernel.SetRadius(float(parameterNode.GetParameter("kernelSharpness")))
        interpolator.SetKernel(gaussianKernel)
      interpolator.SetNullPointsStrategyToClosestPoint()
      interpolator.SetInputData(targetNode.GetPolyData())
      interpolator.SetSourceData(sourceNode.GetPolyData())
      interpolator.Update()
      targetNode.SetAndObservePolyData(interpolator.GetOutput())

    elif parameterNode.GetParameter("type") == "Image to Mesh":
      sourceNode = parameterNode.GetNodeReference("sourceImage")
      interpolator = vtk.vtkImageInterpolator()
      if parameterNode.GetParameter("imageMethod") == "Linear":
        interpolator.SetInterpolationModeToLinear()
      elif parameterNode.GetParameter("imageMethod") == "Nearest":
        interpolator.SetInterpolationModeToNearest()
      else:  # Cubic
        interpolator.SetInterpolationModeToCubic()
      interpolator.Modified()

      # translate by image orgin to match global coordinates
      translation = vtk.vtkTransform()
      origin = sourceNode.GetOrigin()
      translation.Translate(-origin[0], -origin[1], -origin[2])
      transformFilter = vtk.vtkTransformPolyDataFilter()
      transformFilter.SetInputData(targetNode.GetPolyData())
      transformFilter.SetTransform(translation)
      transformFilter.Update()

      probe = vtk.vtkImageProbeFilter()
      probe.SetInterpolator(interpolator)
      probe.SetInputData(transformFilter.GetOutput())
      probe.SetSourceData(sourceNode.GetImageData())
      probe.Update()
      targetNode.GetPolyData().GetPointData().DeepCopy(probe.GetOutput().GetPointData())

    else:  # File to Mesh
      import pandas as pd
      import numpy as np
      from vtk.util.numpy_support import numpy_to_vtk

      df = pd.read_csv(filePath, dtype=float)
      for columnName, column in df.items():
        colunmData = np.array(column.tolist())
        if len(colunmData) != targetNode.GetPolyData().GetNumberOfPoints():
          logging.error("Number of data does not match number of points.")
          return
        vtkArray = numpy_to_vtk(colunmData)
        vtkArray.SetName(columnName)
        vtkArray.SetNumberOfComponents(1)
        targetNode.GetPolyData().GetPointData().AddArray(vtkArray)
        targetNode.GetPolyData().GetPointData().SetActiveScalars(columnName)


class MeshDataImprinterTest(ScriptedLoadableModuleTest):
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
