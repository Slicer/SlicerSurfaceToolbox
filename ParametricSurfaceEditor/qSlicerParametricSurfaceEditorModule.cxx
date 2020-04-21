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

// ParametricSurfaceEditor Logic includes
#include <vtkSlicerParametricSurfaceEditorLogic.h>

// ParametricSurfaceEditor includes
#include "qSlicerParametricSurfaceEditorModule.h"
#include "qSlicerParametricSurfaceEditorModuleWidget.h"

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerParametricSurfaceEditorModulePrivate
{
public:
  qSlicerParametricSurfaceEditorModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerParametricSurfaceEditorModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerParametricSurfaceEditorModulePrivate::qSlicerParametricSurfaceEditorModulePrivate()
= default;

//-----------------------------------------------------------------------------
// qSlicerParametricSurfaceEditorModule methods

//-----------------------------------------------------------------------------
qSlicerParametricSurfaceEditorModule::qSlicerParametricSurfaceEditorModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerParametricSurfaceEditorModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerParametricSurfaceEditorModule::~qSlicerParametricSurfaceEditorModule()
= default;

//-----------------------------------------------------------------------------
QString qSlicerParametricSurfaceEditorModule::helpText() const
{
  return "This module allows surface mesh editing using parametric rules and operations";
}

//-----------------------------------------------------------------------------
QString qSlicerParametricSurfaceEditorModule::acknowledgementText() const
{
  return "This work was partially funded by CANARIE's Research Software Program,"
    "OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.";
}

//-----------------------------------------------------------------------------
QStringList qSlicerParametricSurfaceEditorModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Kyle Sunderland (PerkLab, Queen's)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerParametricSurfaceEditorModule::icon() const
{
  return QIcon(":/Icons/ParametricSurfaceEditor.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerParametricSurfaceEditorModule::categories() const
{
  return QStringList() << "Surface Models";
}

//-----------------------------------------------------------------------------
QStringList qSlicerParametricSurfaceEditorModule::dependencies() const
{
  return QStringList();
}

//-----------------------------------------------------------------------------
void qSlicerParametricSurfaceEditorModule::setup()
{
  this->Superclass::setup();
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerParametricSurfaceEditorModule
::createWidgetRepresentation()
{
  return new qSlicerParametricSurfaceEditorModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerParametricSurfaceEditorModule::createLogic()
{
  return vtkSlicerParametricSurfaceEditorLogic::New();
}
