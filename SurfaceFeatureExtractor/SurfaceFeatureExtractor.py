import os
import unittest
import logging

import numpy as np
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin
from vtk.util.numpy_support import numpy_to_vtk, vtk_to_numpy

#
# SurfaceFeatureExtractor
#

class SurfaceFeatureExtractor(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Surface Feature Extractor"
    self.parent.categories = ["Surface Models"]
    self.parent.dependencies = []
    self.parent.contributors = ["Ye Han, Jean-Christophe Fillion-Robin, Beatriz Paniagua (Kitware)"]
    self.parent.helpText = """
This module supports computing the mass properties (surface area and volume) as well as the surface features (surface 
normal and various types of curvature) from a selected set of input models. Computed surface features can be visualized 
by choosing from the data selection combo box. Note that only the first component will be shown if the selected feature 
is multi-dimensional. Computed mass properties and surface features can be exported to .csv files for further 
postprocessing using the export function.
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
The development of this module was supported by the NIH National Institute of Biomedical Imaging Bioengineering 
R01EB021391 (Shape Analysis Toolbox for Medical Image Computing Projects), 
with help from Andras Lasso (PerkLab, Queen's University) and Steve Pieper (Isomics, Inc.).
"""

#
# SurfaceFeatureExtractorWidget
#

class SurfaceFeatureExtractorWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
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
    uiWidget = slicer.util.loadUI(self.resourcePath('UI/SurfaceFeatureExtractor.ui'))
    self.layout.addWidget(uiWidget)
    self.ui = slicer.util.childWidgetVariables(uiWidget)

    # Set scene in MRML widgets. Make sure that in Qt designer the top-level qMRMLWidget's
    # "mrmlSceneChanged(vtkMRMLScene*)" signal in is connected to each MRML widget's.
    # "setMRMLScene(vtkMRMLScene*)" slot.
    uiWidget.setMRMLScene(slicer.mrmlScene)

    # Create logic class. Logic implements all computations that should be possible to run
    # in batch mode, without a graphical user interface.
    self.logic = SurfaceFeatureExtractorLogic()
    self.logic.updateProcessCallback = self.updateProcess

    # Connections

    # These connections ensure that we update parameter node when scene is closed
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose)

    # These connections ensure that whenever user changes some settings on the GUI, that is saved in the MRML scene
    # (in the selected parameter node).

    self.parameterEditWidgets = [
      # (self.ui.CheckableNodeComboBox_inputNodes, "inputNodes"),  # CheckableNodeComboBox is not yet supported
      (self.ui.checkBox_position, "position"),
      (self.ui.checkBox_normal, "normal"),
      (self.ui.checkBox_minimumCurvature, "minimum curvature"),
      (self.ui.checkBox_maximumCurvature, "maximum curvature"),
      (self.ui.checkBox_gaussianCurvature, "gaussian curvature"),
      (self.ui.checkBox_meanCurvature, "mean curvature"),
      (self.ui.checkBox_shapeIndex, "shape index"),
      (self.ui.checkBox_curvedness, "curvedness"),
      (self.ui.checkBox_surfaceArea, "surface area"),
      (self.ui.checkBox_volume, "volume"),
      (self.ui.comboBox_dataSelection, "data selection"),
      (self.ui.PathLineEdit_exportCSV, "export csv")
    ]

    slicer.util.addParameterEditWidgetConnections(self.parameterEditWidgets, self.updateParameterNodeFromGUI)
    self.ui.CheckableNodeComboBox_inputNodes.connect("checkedNodesChanged()", self.updateGUIFromParameterNode)
    self.ui.comboBox_dataSelection.connect("currentIndexChanged(int)", self.onDataSelectionChanged)

    # Buttons
    self.ui.pushButton_compute.connect('clicked(bool)', self.onComputeButton)
    self.ui.pushButton_export.connect('clicked(bool)', self.onExportButton)
    self.ui.pushButton_export.setEnabled(False)

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
    self.ui.pushButton_compute.setEnabled(not self.ui.CheckableNodeComboBox_inputNodes.noneChecked())

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
    self.ui.pushButton_compute.text = value
    self.ui.pushButton_compute.repaint()

  def updateDataSelectionCombobox(self):
    self.ui.comboBox_dataSelection.disconnect("currentIndexChanged(int)", self.onDataSelectionChanged)
    self.ui.comboBox_dataSelection.clear()
    if self._parameterNode.GetParameter("position") == "true":
      self.ui.comboBox_dataSelection.addItem("Position")
    if self._parameterNode.GetParameter("normal") == "true":
      self.ui.comboBox_dataSelection.addItem("Normals")
    if self._parameterNode.GetParameter("minimum curvature") == "true":
      self.ui.comboBox_dataSelection.addItem("Minimum_Curvature")
    if self._parameterNode.GetParameter("maximum curvature") == "true":
      self.ui.comboBox_dataSelection.addItem("Maximum_Curvature")
    if self._parameterNode.GetParameter("gaussian curvature") == "true":
      self.ui.comboBox_dataSelection.addItem("Gauss_Curvature")
    if self._parameterNode.GetParameter("mean curvature") == "true":
      self.ui.comboBox_dataSelection.addItem("Mean_Curvature")
    if self._parameterNode.GetParameter("shape index") == "true":
      self.ui.comboBox_dataSelection.addItem("Shape_Index")
    if self._parameterNode.GetParameter("curvedness") == "true":
      self.ui.comboBox_dataSelection.addItem("Curvedness")
    if self._parameterNode.GetParameter("surface area") == "true":
      self.ui.comboBox_dataSelection.addItem("Surface_Area")
    if self._parameterNode.GetParameter("volume") == "true":
      self.ui.comboBox_dataSelection.addItem("Volume")
    self.ui.comboBox_dataSelection.connect("currentIndexChanged(int)", self.onDataSelectionChanged)

  def onDataSelectionChanged(self):
    """
    Change active scalar to be visualized on surface models.
    """
    currentText = self.ui.comboBox_dataSelection.currentText
    # Nothing was computed
    if len(currentText) == 0:
      return

    # Population data
    if currentText == "Volume" or currentText == "Surface_Area":
      for node in self.ui.CheckableNodeComboBox_inputNodes.checkedNodes():
        node.GetDisplayNode().ScalarVisibilityOff()
      logging.info(currentText + " is selected and ready for export.")
      return

    # Point data
    scalarRange = [float('inf'), float('-inf')]
    for node in self.ui.CheckableNodeComboBox_inputNodes.checkedNodes():
      pointData = node.GetPolyData().GetPointData()
      dataArray = pointData.GetAbstractArray(currentText)
      if dataArray is not None:
        pointData.SetActiveScalars(currentText)
        nodeScalarRange = pointData.GetScalars().GetRange()
        scalarRange[0] = min(nodeScalarRange[0], scalarRange[0])
        scalarRange[1] = max(nodeScalarRange[1], scalarRange[1])
      else:
        logging.error("Model %s does not have %s computed." % (node.GetName(), currentText))
        return
    for node in self.ui.CheckableNodeComboBox_inputNodes.checkedNodes():
        node.GetDisplayNode().SetActiveScalar(currentText, vtk.vtkAssignAttribute.POINT_DATA)
        node.GetDisplayNode().SetAndObserveColorNodeID("vtkMRMLColorTableNodeRainbow")
        node.GetDisplayNode().SetScalarRangeFlag(0)
        node.GetDisplayNode().SetScalarRange(scalarRange)
        node.GetDisplayNode().SetScalarVisibility(True)

  def onComputeButton(self):
    """
    Run processing when user clicks "Compute" button.
    """
    slicer.app.pauseRender()
    qt.QApplication.setOverrideCursor(qt.Qt.WaitCursor)
    try:
      inputNodes = self.ui.CheckableNodeComboBox_inputNodes.checkedNodes()
      tableWidget = self.ui.tableWidget_modelProperties
      self.logic.computeSurfaceFeatures(inputNodes, self._parameterNode, tableWidget)
      slicer.app.processEvents()
      self.updateDataSelectionCombobox()
      self.onDataSelectionChanged()
      self.ui.pushButton_compute.text = "Compute"
      self.ui.pushButton_export.setEnabled(True)
    except Exception as e:
      slicer.util.errorDisplay("Failed to compute output model: " + str(e))
      import traceback
      traceback.print_exc()
    finally:
      slicer.app.resumeRender()
      qt.QApplication.restoreOverrideCursor()

  def onExportButton(self):
    """
    Export selected mesh property when user clicks "Export" button.
    """
    currentText = self.ui.comboBox_dataSelection.currentText
    currentPath = self.ui.PathLineEdit_exportCSV.currentPath
    if not currentPath.endswith(".csv"):
      currentPath = currentPath + ".csv"
      self.ui.PathLineEdit_exportCSV.currentPath = currentPath

    if currentText == "Volume" or currentText == "Surface_Area":
      self.logic.exportTable(self.ui.tableWidget_modelProperties,
                             currentPath)
    else:
      self.logic.exportMeshProperty(currentText,
                                    currentPath,
                                    self.ui.CheckableNodeComboBox_inputNodes.checkedNodes())
    slicer.util.infoDisplay(currentText + " has been exported to " + self.ui.PathLineEdit_exportCSV.currentPath)


class SurfaceFeatureExtractorLogic(ScriptedLoadableModuleLogic):
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
      ("position", "false"),
      ("normal", "false"),
      ("minimum curvature", "false"),
      ("maximum curvature", "false"),
      ("gaussian curvature", "false"),
      ("mean curvature", "false"),
      ("shape index", "false"),
      ("curvedness", "false"),
      ("surface area", "false"),
      ("volume", "false")
    ]
    for parameterName, defaultValue in defaultValues:
      if not parameterNode.GetParameter(parameterName):
        parameterNode.SetParameter(parameterName, defaultValue)

  def updateProcess(self, message):
    if not self.updateProcessCallback:
      return
    self.updateProcessCallback(message)

  def computeSurfaceFeatures(self, inputNodes, parameterNode, tableWidget):
    # Vectors
    if parameterNode.GetParameter("position") == "true":
      self.computePosition(inputNodes)
    if parameterNode.GetParameter("normal") == "true":
      self.computeNormal(inputNodes)
    # Scalars
    isComputingMinimumCurvature = parameterNode.GetParameter("minimum curvature") == "true"
    isComputingMaximumCurvature = parameterNode.GetParameter("maximum curvature") == "true"
    if isComputingMinimumCurvature or isComputingMaximumCurvature:
      if isComputingMinimumCurvature:
        self.computeMinimumCurvature(inputNodes)
      if isComputingMaximumCurvature:
        self.computeMaximumCurvature(inputNodes)
    else:
      # Mean and Gaussian Curvature will be automatically calculated if minimum or maximum curvatures are requested.
      if parameterNode.GetParameter("gaussian curvature") == "true":
        self.computeGaussianCurvature(inputNodes)
      if parameterNode.GetParameter("mean curvature") == "true":
        self.computeMeanCurvature(inputNodes)
    if parameterNode.GetParameter("shape index") == "true":
      self.computeShapeIndex(inputNodes)
    if parameterNode.GetParameter("curvedness") == "true":
      self.computeCurvedness(inputNodes)
    # Mass properties
    isComputingSurfaceArea = parameterNode.GetParameter("surface area") == "true"
    isComputingVolume = parameterNode.GetParameter("volume") == "true"
    if isComputingSurfaceArea or isComputingVolume:
      self.computeSurfaceAreaAndVolume(inputNodes, isComputingSurfaceArea, isComputingVolume, tableWidget)

  def computePosition(self, inputNodes):
    for inputNode in inputNodes:
      inputPoints = vtk. vtkDoubleArray()
      inputPoints.DeepCopy(inputNode.GetPolyData().GetPoints().GetData())
      inputPoints.SetName("Position")
      inputNode.GetPolyData().GetPointData().AddArray(inputPoints)

  def computeNormal(self, inputNodes):
    for inputNode in inputNodes:
      normal = vtk.vtkPolyDataNormals()
      normal.SetInputData(inputNode.GetPolyData())
      normal.ComputePointNormalsOn()
      normal.ComputeCellNormalsOff()
      normal.SetFlipNormals(0)
      normal.SplittingOff()
      normal.FlipNormalsOff()
      normal.ConsistencyOff()
      normal.Update()
      inputNode.SetAndObservePolyData(normal.GetOutput())

  def computeMinimumCurvature(self, inputNodes):
    for inputNode in inputNodes:
      curvature = vtk.vtkCurvatures()
      curvature.SetInputDataObject(inputNode.GetPolyData())
      curvature.SetCurvatureTypeToMinimum()
      curvature.Update()
      inputNode.SetAndObservePolyData(curvature.GetOutput())

  def computeMaximumCurvature(self, inputNodes):
    for inputNode in inputNodes:
      curvature = vtk.vtkCurvatures()
      curvature.SetInputDataObject(inputNode.GetPolyData())
      curvature.SetCurvatureTypeToMaximum()
      curvature.Update()
      inputNode.SetAndObservePolyData(curvature.GetOutput())

  def computeGaussianCurvature(self, inputNodes):
    for inputNode in inputNodes:
      curvature = vtk.vtkCurvatures()
      curvature.SetInputDataObject(inputNode.GetPolyData())
      curvature.SetCurvatureTypeToGaussian()
      curvature.Update()
      inputNode.SetAndObservePolyData(curvature.GetOutput())

  def computeMeanCurvature(self, inputNodes):
    for inputNode in inputNodes:
      curvature = vtk.vtkCurvatures()
      curvature.SetInputDataObject(inputNode.GetPolyData())
      curvature.SetCurvatureTypeToMean()
      curvature.Update()
      inputNode.SetAndObservePolyData(curvature.GetOutput())

  def computeShapeIndex(self, inputNodes):
    for inputNode in inputNodes:
      minimumCurvatureArray = inputNode.GetPolyData().GetPointData().GetScalars("Minimum_Curvature")
      if not minimumCurvatureArray:
        curvature = vtk.vtkCurvatures()
        curvature.SetInputDataObject(inputNode.GetPolyData())
        curvature.SetCurvatureTypeToMinimum()
        curvature.Update()
        minimumCurvatureArray = curvature.GetOutput().GetPointData().GetScalars("Minimum_Curvature")

      maximumCurvatureArray = inputNode.GetPolyData().GetPointData().GetScalars("Maximum_Curvature")
      if not maximumCurvatureArray:
        curvature = vtk.vtkCurvatures()
        curvature.SetInputDataObject(inputNode.GetPolyData())
        curvature.SetCurvatureTypeToMaximum()
        curvature.Update()
        maximumCurvatureArray = curvature.GetOutput().GetPointData().GetScalars("Maximum_Curvature")

      k1 = vtk_to_numpy(minimumCurvatureArray)
      k2 = vtk_to_numpy(maximumCurvatureArray)
      nPoints = len(k1)
      shapeIndex = np.zeros(nPoints)
      for i in range(nPoints):
        # To avoid nan and divide by zero warnings
        if (k1[i] + k2[i]) == 0:
          shapeIndex[i] = 0
        elif k1[i] == k2[i]:
          shapeIndex[i] = 1 if k1[i] > 0 else -1
        else:
          shapeIndex[i] = (2 / np.pi) * (np.arctan((k2[i] + k1[i]) / (k2[i] - k1[i])))
      shapeIndex = numpy_to_vtk(shapeIndex)
      shapeIndex.SetNumberOfComponents(1)
      shapeIndex.SetName("Shape_Index")
      inputNode.GetPolyData().GetPointData().AddArray(shapeIndex)

  def computeCurvedness(self, inputNodes):
    for inputNode in inputNodes:
      minimumCurvatureArray = inputNode.GetPolyData().GetPointData().GetScalars("Minimum_Curvature")
      if not minimumCurvatureArray:
        curvature = vtk.vtkCurvatures()
        curvature.SetInputDataObject(inputNode.GetPolyData())
        curvature.SetCurvatureTypeToMinimum()
        curvature.Update()
        minimumCurvatureArray = curvature.GetOutput().GetPointData().GetScalars("Minimum_Curvature")

      maximumCurvatureArray = inputNode.GetPolyData().GetPointData().GetScalars("Maximum_Curvature")
      if not maximumCurvatureArray:
        curvature = vtk.vtkCurvatures()
        curvature.SetInputDataObject(inputNode.GetPolyData())
        curvature.SetCurvatureTypeToMaximum()
        curvature.Update()
        maximumCurvatureArray = curvature.GetOutput().GetPointData().GetScalars("Maximum_Curvature")

      k1 = vtk_to_numpy(minimumCurvatureArray)
      k2 = vtk_to_numpy(maximumCurvatureArray)
      curvedness = numpy_to_vtk(np.sqrt((k1 ** 2 + k2 ** 2) / 2))
      curvedness.SetNumberOfComponents(1)
      curvedness.SetName("Curvedness")
      inputNode.GetPolyData().GetPointData().AddArray(curvedness)

  def computeSurfaceAreaAndVolume(self, inputNodes, isComputingSurfaceArea, isComputingVolume, tableWidget):
    tableWidget.clear()
    tableWidget.setRowCount(len(inputNodes))
    tableWidget.setColumnCount(1)
    tableWidget.horizontalHeader().setStretchLastSection(True)
    header = ["Model"]

    surfaceAreas = []
    volumes = []
    for index, inputNode in enumerate(inputNodes):
      item = qt.QTableWidgetItem()
      item.setText(inputNode.GetName())
      item.setFlags(item.flags() & ~qt.Qt.ItemIsEditable)
      tableWidget.setItem(index, 0, item)
      massProperties = vtk.vtkMassProperties()
      massProperties.SetInputData(inputNode.GetPolyData())
      massProperties.Update()
      if isComputingSurfaceArea:
        surfaceAreas.append(massProperties.GetSurfaceArea())
      if isComputingVolume:
        volumes.append(massProperties.GetVolume())

    # Populate the table with computed mass properties
    if isComputingSurfaceArea:
      columnCount = tableWidget.columnCount
      logging.info(str(columnCount))
      tableWidget.setColumnCount(columnCount + 1)
      header.append("Surface Area")
      for index, surfaceArea in enumerate(surfaceAreas):
        item = qt.QTableWidgetItem(f"{surfaceArea:f}")
        item.setFlags(item.flags() & ~qt.Qt.ItemIsEditable)
        tableWidget.setItem(index, columnCount, item)
    if isComputingVolume:
      columnCount = tableWidget.columnCount
      logging.info(str(columnCount))
      tableWidget.setColumnCount(columnCount + 1)
      header.append("Volume")
      for index, volume in enumerate(volumes):
        item = qt.QTableWidgetItem(f"{volume:f}")
        item.setFlags(item.flags() & ~qt.Qt.ItemIsEditable)
        tableWidget.setItem(index, columnCount, item)
    tableWidget.setHorizontalHeaderLabels(header)

  def exportTable(self, tableWidget, csvPath):
    import csv
    with open(csvPath, 'w', newline='') as csvFile:
      csvWriter = csv.writer(csvFile)
      nRow = tableWidget.rowCount
      nColumn = tableWidget.columnCount
      header = []
      for col in range(nColumn):
        header.append(tableWidget.horizontalHeaderItem(col).text())
      csvWriter.writerow(header)

      for row in range(nRow):
        rowText = []
        for col in range(nColumn):
          rowText.append(tableWidget.item(row, col).text())
        csvWriter.writerow(rowText)

  def exportMeshProperty(self, propertyName, csvPath, nodes):
    import csv
    with open(csvPath, 'w', newline='') as csvFile:
      csvWriter = csv.writer(csvFile)
      dataArray0 = nodes[0].GetPolyData().GetPointData().GetAbstractArray(propertyName)
      nComponent = dataArray0.GetNumberOfComponents()
      nRow = dataArray0.GetNumberOfTuples()
      nColumn = nComponent * len(nodes)
      header = []
      data = np.zeros((nRow, nColumn))
      for i, node in enumerate(nodes):
        dataArray = node.GetPolyData().GetPointData().GetAbstractArray(propertyName)
        if nComponent == 1:
          header.append(node.GetName())
          data_i = vtk_to_numpy(dataArray)
          data[:, i] = data_i
        elif nComponent == 3:
          header.append(node.GetName() + "_X")
          header.append(node.GetName() + "_Y")
          header.append(node.GetName() + "_Z")
          data_i = vtk_to_numpy(dataArray)
          data[:, (3 * i):(3 * i + 3)] = data_i
        else:
          logging.error("Exporting point data with dimension other than 1 and 3 is not supported.")
          return
      csvWriter.writerow(header)
      csvWriter.writerows(data)


class SurfaceFeatureExtractorTest(ScriptedLoadableModuleTest):
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
