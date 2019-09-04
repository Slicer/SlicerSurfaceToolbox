import os
import csv
import unittest
import string
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin


class SurfaceToolbox(ScriptedLoadableModule):
  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Surface Toolbox"
    self.parent.categories = ["Surface Models"]
    self.parent.dependencies = []
    self.parent.contributors = ["Luca Antiga (Orobix), Ron Kikinis (Brigham and Women's Hospital), Ben Wilson (Kitware)"] # replace with "Firstname Lastname (Org)"
    self.parent.helpText = """
This module supports various cleanup and optimization processes on surface models.
Select the input and output models, and then enable the stages of the pipeline by selecting the buttons.
Stages that include parameters will open up when they are enabled.
Click apply to activate the pipeline and then click the Toggle button to compare the model before and after the operation.
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
This module was developed by Luca Antiga, Orobix Srl, with a little help from Steve Pieper, Isomics, Inc.
"""


def numericInputFrame(parent, label, tooltip, minimum, maximum, step, decimals):
  inputFrame = qt.QFrame(parent)
  inputFrame.setLayout(qt.QHBoxLayout())
  inputLabel = qt.QLabel(label, inputFrame)
  inputLabel.setToolTip(tooltip)
  inputFrame.layout().addWidget(inputLabel)
  inputSpinBox = qt.QDoubleSpinBox(inputFrame)
  inputSpinBox.setToolTip(tooltip)
  inputSpinBox.minimum = minimum
  inputSpinBox.maximum = maximum
  inputSpinBox.singleStep = step
  inputSpinBox.decimals = decimals
  inputFrame.layout().addWidget(inputSpinBox)
  inputSlider = ctk.ctkDoubleSlider(inputFrame)
  inputSlider.minimum = minimum
  inputSlider.maximum = maximum
  inputSlider.orientation = 1
  inputSlider.singleStep = step
  inputSlider.setToolTip(tooltip)
  inputFrame.layout().addWidget(inputSlider)
  return inputFrame, inputSlider, inputSpinBox


class SurfaceToolboxWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):

  def __init__(self, parent):
    ScriptedLoadableModuleWidget.__init__(self, parent)
    VTKObservationMixin.__init__(self)

  def setup(self):
    ScriptedLoadableModuleWidget.setup(self)

    self.logic = SurfaceToolboxLogic()

    # Instantiate and connect widgets ...
    self.parameterNodeSelector = slicer.qMRMLNodeComboBox()
    self.parameterNodeSelector.nodeTypes = ["vtkMRMLScriptedModuleNode"]
    self.parameterNodeSelector.addAttribute("vtkMRMLScriptedModuleNode", "ModuleName", "SurfaceToolbox")
    self.parameterNodeSelector.selectNodeUponCreation = True
    self.parameterNodeSelector.addEnabled = True
    self.parameterNodeSelector.renameEnabled = True
    self.parameterNodeSelector.removeEnabled = True
    self.parameterNodeSelector.noneEnabled = False
    self.parameterNodeSelector.showHidden = True
    self.parameterNodeSelector.showChildNodeTypes = False
    self.parameterNodeSelector.baseName = "SurfaceToolbox"
    self.parameterNodeSelector.setMRMLScene(slicer.mrmlScene)
    self.parameterNodeSelector.setToolTip("Pick parameter set")
    self.layout.addWidget(self.parameterNodeSelector)

    inputModelSelectorFrame = qt.QFrame(self.parent)
    inputModelSelectorFrame.setLayout(qt.QHBoxLayout())
    self.parent.layout().addWidget(inputModelSelectorFrame)

    inputModelSelectorLabel = qt.QLabel("Input Model: ", inputModelSelectorFrame)
    inputModelSelectorLabel.setToolTip("Select the input model")
    inputModelSelectorFrame.layout().addWidget(inputModelSelectorLabel)

    inputModelSelector = slicer.qMRMLNodeComboBox(inputModelSelectorFrame)
    inputModelSelector.nodeTypes = ["vtkMRMLModelNode"]
    inputModelSelector.selectNodeUponCreation = False
    inputModelSelector.addEnabled = False
    inputModelSelector.removeEnabled = False
    inputModelSelector.noneEnabled = True
    inputModelSelector.showHidden = False
    inputModelSelector.showChildNodeTypes = False
    inputModelSelector.setMRMLScene( slicer.mrmlScene )
    inputModelSelectorFrame.layout().addWidget(inputModelSelector)
    self.inputModelSelector = inputModelSelector

    outputModelSelectorFrame = qt.QFrame(self.parent)
    outputModelSelectorFrame.setLayout(qt.QHBoxLayout())
    self.parent.layout().addWidget(outputModelSelectorFrame)

    outputModelSelectorLabel = qt.QLabel("Output Model: ", outputModelSelectorFrame)
    outputModelSelectorLabel.setToolTip("Select the output model")
    outputModelSelectorFrame.layout().addWidget(outputModelSelectorLabel)

    outputModelSelector = slicer.qMRMLNodeComboBox(outputModelSelectorFrame)
    outputModelSelector.nodeTypes = ["vtkMRMLModelNode"]
    outputModelSelector.selectNodeUponCreation = False
    outputModelSelector.addEnabled = True
    outputModelSelector.renameEnabled = True
    outputModelSelector.removeEnabled = True
    outputModelSelector.noneEnabled = True
    outputModelSelector.showHidden = False
    outputModelSelector.showChildNodeTypes = False
    outputModelSelector.baseName = "Model"
    outputModelSelector.selectNodeUponCreation = True
    outputModelSelector.setMRMLScene(slicer.mrmlScene)
    outputModelSelectorFrame.layout().addWidget(outputModelSelector)
    self.outputModelSelector = outputModelSelector

    # Decimation
    decimationButton = qt.QPushButton("Decimation")
    decimationButton.objectName = "DecimationButton"
    decimationButton.checkable = True
    self.layout.addWidget(decimationButton)
    decimationFrame = qt.QFrame(self.parent)
    self.layout.addWidget(decimationFrame)
    decimationFormLayout = qt.QFormLayout(decimationFrame)

    reductionFrame, reductionSlider, reductionSpinBox = numericInputFrame(
      self.parent, "Reduction:",
      "Specifies the desired reduction in the total number of polygons (e.g., if Reduction is set"
      " to 0.9, this filter will try to reduce the data set to 10% of its original size).", 0.0, 1.0, 0.05, 2)
    decimationFormLayout.addWidget(reductionFrame)

    boundaryDeletionCheckBox = qt.QCheckBox("Boundary deletion")
    decimationFormLayout.addWidget(boundaryDeletionCheckBox)

    # Smoothing
    smoothingButton = qt.QPushButton("Smoothing")
    smoothingButton.objectName = "SmoothingButton"
    smoothingButton.checkable = True
    self.layout.addWidget(smoothingButton)
    smoothingFrame = qt.QFrame(self.parent)
    self.layout.addWidget(smoothingFrame)
    smoothingFormLayout = qt.QFormLayout(smoothingFrame)

    smoothingMethodCombo = qt.QComboBox(smoothingFrame)
    smoothingMethodCombo.addItem("Laplace")
    smoothingMethodCombo.addItem("Taubin")
    smoothingFormLayout.addWidget(smoothingMethodCombo)

    laplaceMethodFrame = qt.QFrame(self.parent)
    smoothingFormLayout.addWidget(laplaceMethodFrame)
    laplaceMethodFormLayout = qt.QFormLayout(laplaceMethodFrame)

    laplaceIterationsFrame, laplaceIterationsSlider, laplaceIterationsSpinBox = numericInputFrame(
      self.parent, "Iterations:",
      "Determines the maximum number of smoothing iterations. Higher value allows more smoothing."
      " In general, small relaxation factors and large numbers of iterations are more stable than"
      "larger relaxation factors and smaller numbers of iterations.", 0.0, 500.0, 1.0, 0)
    laplaceMethodFormLayout.addWidget(laplaceIterationsFrame)

    laplaceRelaxationFrame, laplaceRelaxationSlider, laplaceRelaxationSpinBox = numericInputFrame(
      self.parent, "Relaxation:",
      "Specifies how much points may be displaced during each iteration. "
      "Higher value results in more smoothing.", 0.0, 1.0, 0.1, 1)
    laplaceMethodFormLayout.addWidget(laplaceRelaxationFrame)

    taubinMethodFrame = qt.QFrame(self.parent)
    smoothingFormLayout.addWidget(taubinMethodFrame)
    taubinMethodFormLayout = qt.QFormLayout(taubinMethodFrame)

    taubinIterationsFrame, taubinIterationsSlider, taubinIterationsSpinBox = numericInputFrame(
      self.parent, "Iterations:",
      "Determines the maximum number of smoothing iterations. Higher value allows more accurate smoothing."
      " Typically 10-20 iterations are enough.", 0.0, 100.0, 1.0, 0)
    taubinMethodFormLayout.addWidget(taubinIterationsFrame)

    taubinPassBandFrame, taubinPassBandSlider, taubinPassBandSpinBox = numericInputFrame(
      self.parent, "Pass Band:",
      "Number between 0 and 2. Lower values produce more smoothing.", 0.0, 2.0, 0.0001, 4)
    taubinMethodFormLayout.addWidget(taubinPassBandFrame)

    boundarySmoothingCheckBox = qt.QCheckBox("Boundary Smoothing")
    smoothingFormLayout.addWidget(boundarySmoothingCheckBox)

    # Normals
    normalsButton = qt.QPushButton("Normals")
    normalsButton.objectName = "NormalsButton"
    normalsButton.checkable = True
    self.layout.addWidget(normalsButton)
    normalsFrame = qt.QFrame(self.parent)
    self.layout.addWidget(normalsFrame)
    normalsFormLayout = qt.QFormLayout(normalsFrame)

    autoOrientNormalsCheckBox = qt.QCheckBox("Auto-orient Normals")
    autoOrientNormalsCheckBox.setToolTip("Orient the normals outwards from closed surface")
    normalsFormLayout.addWidget(autoOrientNormalsCheckBox)

    flipNormalsCheckBox = qt.QCheckBox("Flip Normals")
    flipNormalsCheckBox.setToolTip("Flip normal direction from its current or auto-oriented state")
    normalsFormLayout.addWidget(flipNormalsCheckBox)

    splittingCheckBox = qt.QCheckBox("Splitting")
    normalsFormLayout.addWidget(splittingCheckBox)

    featureAngleFrame, featureAngleSlider, featureAngleSpinBox = numericInputFrame(
      self.parent, "Feature Angle:", "Tooltip", 0.0, 180.0, 1.0, 0)
    normalsFormLayout.addWidget(featureAngleFrame)

    # Mirror
    mirrorButton = qt.QPushButton("Mirror")
    mirrorButton.objectName = "MirrorButton"
    mirrorButton.checkable = True
    self.layout.addWidget(mirrorButton)
    mirrorFrame = qt.QFrame(self.parent)
    self.layout.addWidget(mirrorFrame)
    mirrorFormLayout = qt.QFormLayout(mirrorFrame)

    mirrorXCheckBox = qt.QCheckBox("X-axis")
    mirrorXCheckBox.setToolTip("Flip model along its X axis")
    mirrorFormLayout.addWidget(mirrorXCheckBox)

    mirrorYCheckBox = qt.QCheckBox("Y-axis")
    mirrorYCheckBox.setToolTip("Flip model along its Y axis")
    mirrorFormLayout.addWidget(mirrorYCheckBox)

    mirrorZCheckBox = qt.QCheckBox("Z-axis")
    mirrorZCheckBox.setToolTip("Flip model along its Z axis")
    mirrorFormLayout.addWidget(mirrorZCheckBox)

    # Cleaner
    cleanerButton = qt.QPushButton("Cleaner")
    cleanerButton.objectName = "CleanerButton"
    cleanerButton.checkable = True
    self.layout.addWidget(cleanerButton)

    # Fill holes
    fillHolesButton = qt.QPushButton("Fill holes")
    fillHolesButton.objectName = "FillHolesButton"
    fillHolesButton.checkable = True
    self.layout.addWidget(fillHolesButton)

    fillHolesFrame = qt.QFrame(self.parent)
    self.layout.addWidget(fillHolesFrame)
    fillHolesFormLayout = qt.QFormLayout(fillHolesFrame)

    fillHolesSizeFrame, fillHolesSizeSlider, fillHolesSizeSpinBox = numericInputFrame(
      self.parent, "Maximum hole size:",
      "Specifies the maximum size of holes that will be filled. This is represented as a radius to the bounding"
      " circumsphere containing the hole. Note that this is an approximate area; the actual area cannot be"
      " computed without first triangulating the hole. "
      , 0.0, 1000, 0.1, 1)
    fillHolesFormLayout.addWidget(fillHolesSizeFrame)

    # Connectivity
    connectivityButton = qt.QPushButton("Connectivity")
    connectivityButton.objectName = "ConnectivityButton"
    connectivityButton.checkable = True
    self.layout.addWidget(connectivityButton)
    #connectivityFrame = qt.QFrame(self.parent)
    #self.layout.addWidget(connectivityFrame)
    #connectivityFormLayout = qt.QFormLayout(connectivityFrame)

    # TODO: connectivity could be
    # - largest connected
    # - threshold connected (discard below point count)
    # - pick a region interactively
    # - turn a multiple connected surface into a model hierarchy

    # Scale Mesh
    scaleButton = qt.QPushButton("Scale Mesh")
    scaleButton.objectName = "ScaleMeshButton"
    scaleButton.checkable = True
    self.layout.addWidget(scaleButton)
    scaleFrame = qt.QFrame(self.parent)
    self.layout.addWidget(scaleFrame)
    scaleFormLayout = qt.QFormLayout(scaleFrame)

    scaleXFrame, scaleXSlider, scaleXSpinBox = numericInputFrame(
      self.parent, "Scale X:",
      "Specifies the desired scale along an axis.", 0, 50.0, .5, 2)
    scaleFormLayout.addWidget(scaleXFrame)

    scaleYFrame, scaleYSlider, scaleYSpinBox = numericInputFrame(
      self.parent, "Scale Y:",
      "Specifies the desired scale along an axis.", 0, 50.0, .5, 2)
    scaleFormLayout.addWidget(scaleYFrame)

    scaleZFrame, scaleZSlider, scaleZSpinBox = numericInputFrame(
      self.parent, "Scale Z:",
      "Specifies the desired scale along an axis.", 0, 50.0, .5, 2)
    scaleFormLayout.addWidget(scaleZFrame)

    # Translate Mesh
    translateButton = qt.QPushButton("Translate Mesh")
    translateButton.objectName = "TranslateMeshButton"
    translateButton.checkable = True
    self.layout.addWidget(translateButton)
    translateFrame = qt.QFrame(self.parent)
    self.layout.addWidget(translateFrame)
    translateFormLayout = qt.QFormLayout(translateFrame)

    transXFrame, transXSlider, transXSpinBox = numericInputFrame(
      self.parent, "Translate X:",
      "Specifies the desired translation along an axis.", -100.0, 100.0, 5, 2)
    translateFormLayout.addWidget(transXFrame)

    transYFrame, transYSlider, transYSpinBox = numericInputFrame(
      self.parent, "Translate Y:",
      "Specifies the desired translation along an axis.", -100.0, 100.0, 5, 2)
    translateFormLayout.addWidget(transYFrame)

    transZFrame, transZSlider, transZSpinBox = numericInputFrame(
      self.parent, "Translate Z:",
      "Specifies the desired translation along an axis.", -100.0, 100.0, 5, 2)
    translateFormLayout.addWidget(transZFrame)

    # Relax Polygons
    relaxButton = qt.QPushButton("Relax Polygons")
    relaxButton.objectName = "RelaxPolygonsButton"
    relaxButton.checkable = True
    self.layout.addWidget(relaxButton)
    relaxFrame = qt.QFrame(self.parent)
    self.layout.addWidget(relaxFrame)
    relaxFormLayout = qt.QFormLayout(relaxFrame)

    relaxIterationsFrame, relaxIterationsSlider, relaxIterationsSpinBox = numericInputFrame(
      self.parent, "Iterations:",
      "Specifies the desired reduction in the total number of polygons (e.g., if Reduction is set"
      " to 0.9, this filter will try to reduce the data set to 10% of its original size).", 0.0, 100.0, .5, 2)
    relaxFormLayout.addWidget(relaxIterationsFrame)

    # Borders Out
    borderButton = qt.QPushButton("Borders Out")
    borderButton.objectName = "BordersOutButton"
    borderButton.checkable = True
    self.layout.addWidget(borderButton)

    # Translate center to origin.
    originButton = qt.QPushButton("Translate center to origin")
    originButton.objectName = "TranslateCenterToOriginButton"
    originButton.checkable = True
    self.layout.addWidget(originButton)

    buttonFrame = qt.QFrame(self.parent)
    buttonFrame.setLayout(qt.QHBoxLayout())
    self.layout.addWidget(buttonFrame)

    toggleModelsButton = qt.QPushButton("Toggle Models")
    toggleModelsButton.toolTip = "Show original model."
    buttonFrame.layout().addWidget(toggleModelsButton)

    applyButton = qt.QPushButton("Apply")
    applyButton.objectName = "ApplyButton"
    applyButton.toolTip = "Filter surface."
    buttonFrame.layout().addWidget(applyButton)

    self.layout.addStretch(1)

    class state(object):
      processValue = ""
      parameterNode = self.logic.getParameterNode()
      inputParamFile = ""
      outputParamFile = ""
      inputModelNode = None
      outputModelNode = None
      decimation = False
      reduction = 0.8
      boundaryDeletion = False
      smoothing = False
      smoothingMethod = "Laplace"
      laplaceIterations = 100
      laplaceRelaxation = 0.5
      taubinIterations = 30
      taubinPassBand = 0.1
      boundarySmoothing = True
      normals = False
      flipNormals = False
      autoOrientNormals = False
      mirror = False
      mirrorX = False
      mirrorY = False
      mirrorZ = False
      splitting = False
      featureAngle = 30.0
      cleaner = False
      fillHoles = False
      fillHolesSize = 1000.0
      connectivity = False
      scale = False
      scaleX = 0.5
      scaleY = 0.5
      scaleZ = 0.5
      translate = False
      transX = 0
      transY = 0
      transZ = 0
      relax = False
      relaxIterations = 0
      border = False
      origin = False

    scope_locals = locals()

    def connect(obj, evt, cmd):
      def callback(*args):
        current_locals = scope_locals.copy()
        current_locals.update({'args': args})
        exec(cmd, globals(), current_locals)
        updateGUI()
      obj.connect(evt, callback)

    def updateGUI():

      def button_stylesheet(active):
        if active:
          return "background-color: green"
        else:
          return ""

      decimationButton.checked = state.decimation
      #decimationButton.setStyleSheet(button_stylesheet(state.decimation))
      decimationFrame.visible = state.decimation
      boundaryDeletionCheckBox.checked = state.boundaryDeletion
      reductionSlider.value = state.reduction
      reductionSpinBox.value = state.reduction

      smoothingButton.checked = state.smoothing
      smoothingFrame.visible = state.smoothing
      laplaceMethodFrame.visible = state.smoothingMethod == "Laplace"
      laplaceIterationsSlider.value = state.laplaceIterations
      laplaceIterationsSpinBox.value = state.laplaceIterations
      laplaceRelaxationSlider.value = state.laplaceRelaxation
      laplaceRelaxationSpinBox.value = state.laplaceRelaxation
      taubinMethodFrame.visible = state.smoothingMethod == "Taubin"
      taubinIterationsSlider.value = state.taubinIterations
      taubinIterationsSpinBox.value = state.taubinIterations
      taubinPassBandSlider.value = state.taubinPassBand
      taubinPassBandSpinBox.value = state.taubinPassBand
      boundarySmoothingCheckBox.checked = state.boundarySmoothing

      normalsButton.checked = state.normals
      normalsFrame.visible = state.normals
      autoOrientNormalsCheckBox.checked = state.autoOrientNormals
      flipNormalsCheckBox.checked = state.flipNormals
      splittingCheckBox.checked = state.splitting
      featureAngleFrame.visible = state.splitting
      featureAngleSlider.value = state.featureAngle
      featureAngleSpinBox.value = state.featureAngle
      featureAngleFrame.visible = state.splitting

      mirrorButton.checked = state.mirror
      mirrorFrame.visible = state.mirror
      mirrorXCheckBox.checked = state.mirrorX
      mirrorYCheckBox.checked = state.mirrorY
      mirrorZCheckBox.checked = state.mirrorZ

      cleanerButton.checked = state.cleaner

      fillHolesButton.checked = state.fillHoles
      fillHolesFrame.visible = state.fillHoles
      fillHolesSizeSlider.value = state.fillHolesSize
      fillHolesSizeSpinBox.value = state.fillHolesSize

      connectivityButton.checked = state.connectivity

      scaleButton.checked = state.scale
      scaleFrame.visible = state.scale
      scaleXSlider.value = state.scaleX
      scaleXSpinBox.value = state.scaleX
      scaleYSlider.value = state.scaleY
      scaleYSpinBox.value = state.scaleY
      scaleZSlider.value = state.scaleZ
      scaleZSpinBox.value = state.scaleZ

      translateButton.checked = state.translate
      translateFrame.visible = state.translate
      transXSlider.value = state.transX
      transXSpinBox.value = state.transX
      transYSlider.value = state.transY
      transYSpinBox.value = state.transY
      transZSlider.value = state.transZ
      transZSpinBox.value = state.transZ

      relaxButton.checked = state.relax
      relaxFrame.visible = state.relax
      relaxIterationsSlider.value = state.relaxIterations
      relaxIterationsSpinBox.value = state.relaxIterations

      borderButton.checked = state.border

      originButton.checked = state.origin

      toggleModelsButton.enabled = state.inputModelNode is not None and state.outputModelNode is not None
      applyButton.enabled = state.inputModelNode is not None and state.outputModelNode is not None

    connect(inputModelSelector, 'currentNodeChanged(vtkMRMLNode*)', 'state.inputModelNode = args[0]')
    connect(outputModelSelector, 'currentNodeChanged(vtkMRMLNode*)', 'state.outputModelNode = args[0]')

    def checkDefine(value, parameter):
      if str(parameter) == "False":
        return False
      if str(parameter) == "True":
        return True
      if parameter == "" or (str(value) == str(parameter)):
        return value
      else:
        return parameter

    def updateGuiFromMRML(node):
      if node is None:
        return
      state.decimation = checkDefine(state.decimation, node.GetParameter("Decimation"))
      state.reduction = float(checkDefine(state.reduction, node.GetParameter("DecimateReduction")))
      state.boundaryDeletion = checkDefine(state.boundaryDeletion, node.GetParameter("DecimateBoundary"))
      state.smoothing = checkDefine(state.smoothing, node.GetParameter("smoothing"))
      state.smoothingMethod = checkDefine(state.smoothingMethod, node.GetParameter("smoothingMethod"))
      state.laplaceIterations = int(checkDefine(state.laplaceIterations, node.GetParameter("SmoothingLaplaceIterations")))
      state.laplaceRelaxation = float(checkDefine(state.laplaceIterations, node.GetParameter("SmoothingLaplaceRelaxation")))
      state.taubinIterations = int(checkDefine(state.taubinIterations, node.GetParameter("SmoothingTaubinIterations")))
      state.taubinPassBand = float(checkDefine(state.taubinPassBand, node.GetParameter("SmoothingTaubinPassBand")))
      state.boundarySmoothing = checkDefine(state.boundarySmoothing, node.GetParameter("SmoothingTaubinBoundary"))
      state.normals = checkDefine(state.normals, node.GetParameter("normals"))
      state.flipNormals = checkDefine(state.flipNormals, node.GetParameter("NormalsFlip"))
      state.autoOrientNormals = checkDefine(state.autoOrientNormals, node.GetParameter("NormalsOrient"))
      state.mirror = checkDefine(state.mirror, node.GetParameter("mirror"))
      state.mirrorX = checkDefine(state.mirrorX, node.GetParameter("mirrorX"))
      state.mirrorY = checkDefine(state.mirrorY, node.GetParameter("mirrorY"))
      state.mirrorZ = checkDefine(state.mirrorZ, node.GetParameter("mirrorZ"))
      state.splitting = checkDefine(state.splitting, node.GetParameter("NormalsSplitting"))
      state.featureAngle = float(checkDefine(state.featureAngle, node.GetParameter("NormalsAngle")))
      state.cleaner = checkDefine(state.cleaner, node.GetParameter("cleaner"))
      state.fillHoles = checkDefine(state.fillHoles, node.GetParameter("fillHoles"))
      state.fillHolesSize = float(checkDefine(state.fillHolesSize, node.GetParameter("fillHolesSize")))
      state.connectivity = checkDefine(state.connectivity, node.GetParameter("connectivity"))
      state.scale = checkDefine(state.scale, node.GetParameter("scale"))
      state.scaleX = float(checkDefine(state.scaleX, node.GetParameter("ScaleDimX")))
      state.scaleY = float(checkDefine(state.scaleY, node.GetParameter("ScaleDimY")))
      state.scaleZ = float(checkDefine(state.scaleZ, node.GetParameter("ScaleDimZ")))
      state.translate = checkDefine(state.translate, node.GetParameter("translate"))
      state.transX = float(checkDefine(state.transX, node.GetParameter("TransDimX")))
      state.transY = float(checkDefine(state.transY, node.GetParameter("TransDimY")))
      state.transZ = float(checkDefine(state.transZ, node.GetParameter("TransDimZ")))
      state.relax = checkDefine(state.relax, node.GetParameter("relax"))
      state.relaxIterations = float(checkDefine(state.relaxIterations, node.GetParameter("RelaxIterations")))
      state.border = checkDefine(state.border, node.GetParameter("border"))
      state.origin = checkDefine(state.origin, node.GetParameter("origin"))
      updateGUI()

    def initializeModelNode(node):
      displayNode = slicer.vtkMRMLModelDisplayNode()
      storageNode = slicer.vtkMRMLModelStorageNode()
      displayNode.SetScene(slicer.mrmlScene)
      storageNode.SetScene(slicer.mrmlScene)
      slicer.mrmlScene.AddNode(displayNode)
      slicer.mrmlScene.AddNode(storageNode)
      node.SetAndObserveDisplayNodeID(displayNode.GetID())
      node.SetAndObserveStorageNodeID(storageNode.GetID())

    self.parameterNodeSelector.setCurrentNode(self.logic.getParameterNode())
    self.parameterNodeSelector.connect('currentNodeChanged(vtkMRMLNode*)', updateGuiFromMRML)
    self.parameterNodeSelector.connect('nodeAdded(vtkMRMLNode*)', updateGuiFromMRML)
    connect(self.parameterNodeSelector, 'currentNodeChanged(vtkMRMLNode*)', 'state.parameterNode = args[0]')

    outputModelSelector.connect('nodeAddedByUser(vtkMRMLNode*)', initializeModelNode)

    connect(decimationButton, 'clicked(bool)', 'state.decimation = args[0]')
    connect(reductionSlider, 'valueChanged(double)', 'state.reduction = args[0]')
    connect(reductionSpinBox, 'valueChanged(double)', 'state.reduction = args[0]')
    connect(boundaryDeletionCheckBox, 'stateChanged(int)', 'state.boundaryDeletion = bool(args[0])')

    connect(smoothingButton, 'clicked(bool)', 'state.smoothing = args[0]')
    connect(smoothingMethodCombo, 'currentIndexChanged(QString)', 'state.smoothingMethod = args[0]')

    connect(laplaceIterationsSlider, 'valueChanged(double)', 'state.laplaceIterations = int(args[0])')
    connect(laplaceIterationsSpinBox, 'valueChanged(double)', 'state.laplaceIterations = int(args[0])')
    connect(laplaceRelaxationSlider, 'valueChanged(double)', 'state.laplaceRelaxation = args[0]')
    connect(laplaceRelaxationSpinBox, 'valueChanged(double)', 'state.laplaceRelaxation = args[0]')

    connect(taubinIterationsSlider, 'valueChanged(double)', 'state.taubinIterations = int(args[0])')
    connect(taubinIterationsSpinBox, 'valueChanged(double)', 'state.taubinIterations = int(args[0])')
    connect(taubinPassBandSlider, 'valueChanged(double)', 'state.taubinPassBand = args[0]')
    connect(taubinPassBandSpinBox, 'valueChanged(double)', 'state.taubinPassBand = args[0]')

    connect(boundarySmoothingCheckBox, 'stateChanged(int)', 'state.boundarySmoothing = bool(args[0])')

    connect(normalsButton, 'clicked(bool)', 'state.normals = args[0]')
    connect(autoOrientNormalsCheckBox, 'toggled(bool)', 'state.autoOrientNormals = bool(args[0])')
    connect(flipNormalsCheckBox, 'toggled(bool)', 'state.flipNormals = bool(args[0])')
    connect(splittingCheckBox, 'stateChanged(int)', 'state.splitting = bool(args[0])')
    connect(featureAngleSlider, 'valueChanged(double)', 'state.featureAngle = args[0]')
    connect(featureAngleSpinBox, 'valueChanged(double)', 'state.featureAngle = args[0]')

    connect(mirrorButton, 'clicked(bool)', 'state.mirror = args[0]')
    connect(mirrorXCheckBox, 'toggled(bool)', 'state.mirrorX = bool(args[0])')
    connect(mirrorYCheckBox, 'toggled(bool)', 'state.mirrorY = bool(args[0])')
    connect(mirrorZCheckBox, 'toggled(bool)', 'state.mirrorZ = bool(args[0])')

    connect(cleanerButton, 'clicked(bool)', 'state.cleaner = args[0]')

    connect(fillHolesButton, 'clicked(bool)', 'state.fillHoles = args[0]')
    connect(fillHolesSizeSlider, 'valueChanged(double)', 'state.fillHolesSize = args[0]')
    connect(fillHolesSizeSpinBox, 'valueChanged(double)', 'state.fillHolesSize = args[0]')

    connect(connectivityButton, 'clicked(bool)', 'state.connectivity = args[0]')

    connect(scaleButton, 'clicked(bool)', 'state.scale = args[0]')
    connect(scaleXSlider, 'valueChanged(double)', 'state.scaleX = args[0]')
    connect(scaleXSpinBox, 'valueChanged(double)', 'state.scaleX = args[0]')
    connect(scaleYSlider, 'valueChanged(double)', 'state.scaleY = args[0]')
    connect(scaleYSpinBox, 'valueChanged(double)', 'state.scaleY = args[0]')
    connect(scaleZSlider, 'valueChanged(double)', 'state.scaleZ = args[0]')
    connect(scaleZSpinBox, 'valueChanged(double)', 'state.scaleZ = args[0]')

    connect(translateButton, 'clicked(bool)', 'state.translate = args[0]')
    connect(transXSlider, 'valueChanged(double)', 'state.transX = args[0]')
    connect(transXSpinBox, 'valueChanged(double)', 'state.transX = args[0]')
    connect(transYSlider, 'valueChanged(double)', 'state.transY = args[0]')
    connect(transYSpinBox, 'valueChanged(double)', 'state.transY = args[0]')
    connect(transZSlider, 'valueChanged(double)', 'state.transZ = args[0]')
    connect(transZSpinBox, 'valueChanged(double)', 'state.transZ = args[0]')

    connect(relaxButton, 'clicked(bool)', 'state.relax = args[0]')
    connect(relaxIterationsSlider, 'valueChanged(double)', 'state.relaxIterations = args[0]')
    connect(relaxIterationsSpinBox, 'valueChanged(double)', 'state.relaxIterations = args[0]')

    connect(borderButton, 'clicked(bool)', 'state.border = args[0]')

    connect(originButton, 'clicked(bool)', 'state.origin = args[0]')

    def updateProcess(value):
      """Display changing process value"""
      updateGUI()
      if state.processValue != "Apply":
          applyButton.text = value
      applyButton.repaint()
      return

    def onApply():
      logic = SurfaceToolboxLogic()
      result = logic.applyFilters(state, updateProcess)
      slicer.app.processEvents()
      if result:
        state.inputModelNode.GetModelDisplayNode().VisibilityOff()
        state.outputModelNode.GetModelDisplayNode().VisibilityOn()
      else:
        state.inputModelNode.GetModelDisplayNode().VisibilityOn()
        state.outputModelNode.GetModelDisplayNode().VisibilityOff()
      updateGUI()
      applyButton.text = "Apply"

    applyButton.connect('clicked()', onApply)

    def onToggleModels():
      updateGUI()
      if state.inputModelNode.GetModelDisplayNode().GetVisibility():
        state.inputModelNode.GetModelDisplayNode().VisibilityOff()
        state.outputModelNode.GetModelDisplayNode().VisibilityOn()
        toggleModelsButton.text = "Toggle Models (Output)"
      else:
        state.inputModelNode.GetModelDisplayNode().VisibilityOn()
        state.outputModelNode.GetModelDisplayNode().VisibilityOff()
        toggleModelsButton.text = "Toggle Models (Input)"

    toggleModelsButton.connect('clicked()', onToggleModels)

    updateGUI()

    self.updateGUI = updateGUI

    # Connect observers to scene events
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndImportEvent, self.onSceneEndImport)

  def enter(self):
    """Runs whenever the module is reopened
    """
    # Set parameter set node if absent
    self.selectParameterNode()

  def selectParameterNode(self):
    if self.parameterNodeSelector.currentNode() is None:
      parameterNode = self.logic.getParameterNode()
      self.parameterNodeSelector.setCurrentNodeID(parameterNode.GetID())

  def onSceneStartClose(self, caller, event):
    pass

  def onSceneEndClose(self, caller, event):
    if self.parent.isEntered:
      self.selectParameterNode()

  def onSceneEndImport(self, caller, event):
    if self.parent.isEntered:
      self.selectParameterNode()

  def cleanup(self):
    self.removeObservers()


class SurfaceToolboxLogic(ScriptedLoadableModuleLogic):
  """Perform filtering
  """

  def __init__(self, parent=None):
    ScriptedLoadableModuleLogic.__init__(self, parent)
    self.isSingletonParameterNode = False

  @staticmethod
  def parameterDefine(state, parameter, value):
    """Define a parameter if not already defined
    """
    if state.parameterNode.GetParameter(str(parameter)) == "" or state.parameterNode.GetParameter(str(parameter)) != value:
      state.parameterNode.SetParameter(str(parameter), str(value))

  @staticmethod
  def loadParameters(state):
    parameterNames = state.parameterNode.GetParameterNamesAsCommaSeparatedList().split(',')
    for parameterName in parameterNames:
      if not parameterName.startswith("ModuleName") and parameterName != "":
        state.parameterNode.SetAttribute(parameterName, state.parameterNode.GetParameter(str(parameterName)))

  @staticmethod
  def saveParameters(state):
    parameterNames = state.parameterNode.GetParameterNamesAsCommaSeparatedList().split(',')
    for parameterName in parameterNames:

      if not parameterName.startswith("ModuleName"): #and (state.parameterNode.GetParameter(str(i)) != state.parameterNode.GetParameter((str(i))))):
        state.parameterNode.SetAttribute(parameterName, state.parameterNode.GetParameter(str(parameterName)))

  def applyFilters(self, state, updateProcess):
    self.loadParameters(state)
    surface = state.inputModelNode.GetPolyDataConnection()

    self.parameterDefine(state, "inputVolume", state.inputModelNode.GetID())
    self.parameterDefine(state, "outputVolume", state.outputModelNode.GetID())

    # define which selections were made
    self.parameterDefine(state, "decimation", state.decimation)
    self.parameterDefine(state, "smoothing", state.smoothing)
    self.parameterDefine(state, "normals", state.normals)
    self.parameterDefine(state, "mirror", state.mirror)
    self.parameterDefine(state, "cleaner", state.cleaner)
    self.parameterDefine(state, "fillHoles", state.fillHoles)
    self.parameterDefine(state, "connectivity",state.connectivity)
    self.parameterDefine(state, "scale", state.scale)
    self.parameterDefine(state, "translate", state.translate)
    self.parameterDefine(state, "relax", state.relax)
    self.parameterDefine(state, "border", state.border)
    self.parameterDefine(state, "origin", state.origin)

    if str(state.parameterNode.GetParameter("decimation")) == "True":
      state.processValue = "Decimation..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "DecimateReduction", str(state.reduction))
      self.parameterDefine(state, "DecimateBoundary", str(state.boundaryDeletion))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "Decimate": float(state.parameterNode.GetParameter("DecimateReduction")),
                    "Boundary": bool(state.parameterNode.GetParameter("DecimateBoundary"))}
      decimationMaker = slicer.modules.decimation
      slicer.cli.runSync(decimationMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("smoothing")) == "True":
      state.processValue = "Smoothing..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "SmoothingLaplaceBoundary", str(state.boundarySmoothing))
      self.parameterDefine(state, "SmoothingLaplaceIterations", str(state.laplaceIterations))
      self.parameterDefine(state, "SmoothingLaplaceRelaxation", str(state.laplaceRelaxation))

      self.parameterDefine(state, "SmoothingTaubinBoundary", str(state.boundarySmoothing))
      self.parameterDefine(state, "SmoothingTaubinIterations", str(state.taubinIterations))
      self.parameterDefine(state, "SmoothingTaubinPassBand", str(state.taubinPassBand))
      self.parameterDefine(state, "smoothingMethod", str(state.smoothingMethod))

      # Keeping default python.
      if state.parameterNode.GetParameter("smoothingMethod") == "Laplace":
        smoothing = vtk.vtkSmoothPolyDataFilter()
        smoothing.SetBoundarySmoothing(bool(state.parameterNode.GetParameter("SmoothingLaplaceBoundary")))
        smoothing.SetNumberOfIterations(int(state.parameterNode.GetParameter("SmoothingLaplaceIterations")))
        smoothing.SetRelaxationFactor(float(state.parameterNode.GetParameter("SmoothingLaplaceRelaxation")))
        smoothing.SetInputConnection(surface)
        surface = smoothing.GetOutputPort()
      elif state.parameterNode.GetParameter("smoothingMethod"):
        smoothing = vtk.vtkWindowedSincPolyDataFilter()
        smoothing.SetBoundarySmoothing(bool(state.parameterNode.GetParameter("SmoothingTaubinBoundary")))
        smoothing.SetNumberOfIterations(int(state.parameterNode.GetParameter("SmoothingTaubinIterations")))
        smoothing.SetPassBand(int(state.parameterNode.GetParameter("SmoothingTaubinPassBand")))
        smoothing.SetInputConnection(surface)
        surface = smoothing.GetOutputPort()

    if str(state.parameterNode.GetParameter("normals")) == "True":
      state.processValue = "Normals..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "NormalsOrient", str(state.autoOrientNormals))
      self.parameterDefine(state, "NormalsFlip", str(state.flipNormals))
      self.parameterDefine(state, "NormalsSplitting", str(state.splitting))
      self.parameterDefine(state, "NormalsAngle", str(state.featureAngle))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "orient": bool(state.parameterNode.GetParameter("NormalsOrient")),
                    "flip": bool(state.parameterNode.GetParameter("NormalsFlip")),
                    "splitting": bool(state.parameterNode.GetParameter("NormalsAngle")), "angle": state.featureAngle}

      normalsMaker = slicer.modules.normals
      slicer.cli.runSync(normalsMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("mirror"))  == "True":
      state.processValue = "Mirror..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "MirrorxAxis", str(state.mirrorX))
      self.parameterDefine(state, "MirroryAxis", str(state.mirrorY))
      self.parameterDefine(state, "MirrorzAxis", str(state.mirrorZ))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "xAxis": bool(state.parameterNode.GetParameter("MirrorxAxis")),
                    "yAxis": bool(state.parameterNode.GetParameter("MirroryAxis")),
                    "zAxis": bool(state.parameterNode.GetParameter("MirrorzAxis"))}
      mirrorMaker = slicer.modules.mirror
      slicer.cli.runSync(mirrorMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("cleaner")) == "True":
      state.processValue = "Cleaner..."
      updateProcess(state.processValue)
      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume")}
      cleanerMaker = slicer.modules.cleaner
      slicer.cli.runSync(cleanerMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("fillHoles")) == "True":
      state.processValue = "Fill Holes..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "HolesMaximum", str(state.fillHolesSize))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "holes": float(state.parameterNode.GetParameter("HolesMaximum"))}
      fillHolesMaker = slicer.modules.fillholes
      slicer.cli.runSync(fillHolesMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("connectivity")) == "True":
      state.processValue = "Connectivity..."
      updateProcess(state.processValue)
      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume")}
      connectivityMaker = slicer.modules.connectivity
      slicer.cli.runSync(connectivityMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("scale")) == "True":
      state.processValue = "Scale..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "ScaleDimX", str(state.scaleX))
      self.parameterDefine(state, "ScaleDimY", str(state.scaleY))
      self.parameterDefine(state, "ScaleDimZ", str(state.scaleZ))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "dimX": float(state.parameterNode.GetParameter("ScaleDimX")),
                    "dimY": float(state.parameterNode.GetParameter("ScaleDimY")),
                    "dimZ": float(state.parameterNode.GetParameter("ScaleDimZ"))}
      scaleMaker = slicer.modules.scalemesh
      slicer.cli.runSync(scaleMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("translate")) == "True":
      state.processValue = "Translating..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "TransDimX", str(state.transX))
      self.parameterDefine(state, "TransDimY", str(state.transY))
      self.parameterDefine(state, "TransDimZ", str(state.transZ))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "dimX": float(state.parameterNode.GetParameter("TransDimX")),
                    "dimY": float(state.parameterNode.GetParameter("TransDimY")),
                    "dimZ": float(state.parameterNode.GetParameter("TransDimZ"))}
      transMaker = slicer.modules.translatemesh
      slicer.cli.runSync(transMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("relax")) == "True":
      state.processValue = "Relaxing..."
      updateProcess(state.processValue)

      self.parameterDefine(state, "RelaxIterations", str(state.relaxIterations))

      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume"),
                    "Iterations": float(state.parameterNode.GetParameter("RelaxIterations"))}
      relaxMaker = slicer.modules.relaxpolygons
      slicer.cli.runSync(relaxMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("border")) == "True":
      state.processValue = "Changing Borders..."
      updateProcess(state.processValue)
      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume")}
      borderMaker = slicer.modules.bordersout
      slicer.cli.runSync(borderMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    if str(state.parameterNode.GetParameter("origin")) == "True":
      state.processValue = "Moving Origin..."
      updateProcess(state.processValue)
      parameters = {"inputVolume": state.parameterNode.GetParameter("inputVolume"),
                    "outputVolume": state.parameterNode.GetParameter("outputVolume")}
      originMaker = slicer.modules.mc2origin
      slicer.cli.runSync(originMaker, None, parameters)
      surface = state.outputModelNode.GetPolyDataConnection()

    state.outputModelNode.SetPolyDataConnection(surface)
    state.processValue = "Apply"
    updateProcess(state.processValue)

    self.saveParameters(state)
    return True


class SurfaceToolboxTest(ScriptedLoadableModuleTest):
  """
  This is the test case for your scripted module.
  """

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
    self.test_SurfaceToolbox1()
    self.setUp()
    self.test_SurfaceToolbox2()

  def test_SurfaceToolbox1(self):
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
    SampleData.downloadFromURL(
      nodeNames='cow',
      fileNames='cow.vtp',
      loadFileTypes='ModelFile',
      uris='https://github.com/Slicer/SlicerTestingData/releases/download/SHA256/d5aa4901d186902f90e17bf3b5917541cb6cb8cf223bfeea736631df4c047652',
      checksums='SHA256:d5aa4901d186902f90e17bf3b5917541cb6cb8cf223bfeea736631df4c047652')
    self.delayDisplay('Finished with download and loading')

    modelNode = slicer.util.getNode(pattern="cow_1")

    logic = SurfaceToolboxLogic()
    self.assertIsNotNone(logic)

    slicer.util.selectModule(slicer.modules.surfacetoolbox)
    slicer.modules.SurfaceToolboxWidget.inputModelSelector.setCurrentNode(modelNode)
    outputModelNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLModelNode", "output")
    slicer.modules.SurfaceToolboxWidget.outputModelSelector.setCurrentNode(outputModelNode)
    self.delayDisplay('Module selected, input and output configured')

    for buttonName in [
      "DecimationButton",
      "SmoothingButton",
      "NormalsButton",
      "MirrorButton",
      "CleanerButton",
      "FillHolesButton",
      "ConnectivityButton",
      "ScaleMeshButton",
      "TranslateMeshButton",
      "RelaxPolygonsButton",
      "BordersOutButton",
      "TranslateCenterToOriginButton"
    ]:
      button = slicer.util.findChild(slicer.modules.SurfaceToolboxWidget.parent, buttonName)
      if not button.checked:
        button.click()

    applyButton = slicer.util.findChild(slicer.modules.SurfaceToolboxWidget.parent, "ApplyButton")
    applyButton.click()

    self.delayDisplay('Test passed!')

  def test_SurfaceToolbox2(self):
    """Re-run first test to ensure using the module after clearing the scene works as expected"""
    self.test_SurfaceToolbox1()
