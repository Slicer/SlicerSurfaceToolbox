/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, Cancer
  Care Ontario, OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.

==============================================================================*/

#ifndef __qSlicerDynamicModelerModuleWidget_h
#define __qSlicerDynamicModelerModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerDynamicModelerModuleExport.h"

class qSlicerDynamicModelerModuleWidgetPrivate;
class vtkMRMLNode;
class vtkSlicerDynamicModelerRule;

/// \ingroup Slicer_QtModules_ExtensionTemplate
class Q_SLICER_QTMODULES_DYNAMICMODELER_EXPORT qSlicerDynamicModelerModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT;

public:
  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerDynamicModelerModuleWidget(QWidget *parent=0);
  ~qSlicerDynamicModelerModuleWidget() override;

public:
  // TODO
  void addRuleButton(QIcon icon, vtkSlicerDynamicModelerRule* rule);

protected:
  QScopedPointer<qSlicerDynamicModelerModuleWidgetPrivate> d_ptr;

  void setup() override;

  void resetInputWidgets();
  void resetParameterWidgets();
  void resetOutputWidgets();

  void updateInputWidgets();
  void updateParameterWidgets();
  void updateOutputWidgets();

protected slots:
  void onAddRuleClicked();
  void onParameterNodeChanged();
  void updateWidgetFromMRML();
  void updateMRMLFromWidget();
  void onApplyButtonClicked();

private:
  Q_DECLARE_PRIVATE(qSlicerDynamicModelerModuleWidget);
  Q_DISABLE_COPY(qSlicerDynamicModelerModuleWidget);
};

#endif
