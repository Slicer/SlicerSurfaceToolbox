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

// DynamicModeler Logic includes
#include <vtkSlicerDynamicModelerLogic.h>

// DynamicModeler includes
#include "qSlicerDynamicModelerModule.h"
#include "qSlicerDynamicModelerModuleWidget.h"

// DynamicModeler subject hierarchy includes
#include "qSlicerSubjectHierarchyDynamicModelerPlugin.h"

// Subject hierarchy includes
#include <qSlicerSubjectHierarchyPluginHandler.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerDynamicModelerModulePrivate
{
public:
  qSlicerDynamicModelerModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerDynamicModelerModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerDynamicModelerModulePrivate::qSlicerDynamicModelerModulePrivate()
= default;

//-----------------------------------------------------------------------------
// qSlicerDynamicModelerModule methods

//-----------------------------------------------------------------------------
qSlicerDynamicModelerModule::qSlicerDynamicModelerModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerDynamicModelerModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerDynamicModelerModule::~qSlicerDynamicModelerModule()
= default;

//-----------------------------------------------------------------------------
QString qSlicerDynamicModelerModule::helpText() const
{
  return "This module allows surface mesh editing using dynamic modelling rules and operations";
}

//-----------------------------------------------------------------------------
QString qSlicerDynamicModelerModule::acknowledgementText() const
{
  return "This work was partially funded by CANARIE's Research Software Program,"
    "OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.";
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModelerModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Kyle Sunderland (PerkLab, Queen's)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerDynamicModelerModule::icon() const
{
  return QIcon(":/Icons/DynamicModeler.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModelerModule::categories() const
{
  return QStringList() << "Surface Models";
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModelerModule::dependencies() const
{
  return QStringList();
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModelerModule::setup()
{
  this->Superclass::setup();

  vtkSlicerDynamicModelerLogic* dynamicModelerLogic = vtkSlicerDynamicModelerLogic::SafeDownCast(this->logic());

  // Register Subject Hierarchy core plugins
  qSlicerSubjectHierarchyDynamicModelerPlugin* dynamicModelerPlugin = new qSlicerSubjectHierarchyDynamicModelerPlugin();
  dynamicModelerPlugin->setDynamicModelerLogic(dynamicModelerLogic);
  qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(dynamicModelerPlugin);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerDynamicModelerModule
::createWidgetRepresentation()
{
  return new qSlicerDynamicModelerModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerDynamicModelerModule::createLogic()
{
  return vtkSlicerDynamicModelerLogic::New();
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModelerModule::associatedNodeTypes() const
{
  return QStringList()
    << "vtkMRMLAnnotationFiducialNode";
}