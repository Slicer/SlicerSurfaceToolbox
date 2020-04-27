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

// .NAME vtkSlicerDynamicModelerLogic - slicer logic class for volumes manipulation
// .SECTION Description
// This class manages the logic associated with reading, saving,
// and changing propertied of the volumes


#ifndef __vtkSlicerDynamicModelerLogic_h
#define __vtkSlicerDynamicModelerLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"

// Logic includes
#include <vtkSlicerDynamicModelerRule.h>

// STD includes
#include <cstdlib>

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkMRMLDynamicModelerNode;

/// \ingroup Slicer_QtModules_ExtensionTemplate
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerLogic :
  public vtkSlicerModuleLogic
{
public:

  static vtkSlicerDynamicModelerLogic *New();
  vtkTypeMacro(vtkSlicerDynamicModelerLogic, vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Returns the current rule object that is being used with the surface editor node
  vtkSlicerDynamicModelerRule* GetDynamicModelerRule(vtkMRMLDynamicModelerNode* surfaceEditorNode);

  /// Run the editor rule specified by the surface editor node
  void RunDynamicModelerRule(vtkMRMLDynamicModelerNode* surfaceEditorNode);

  /// Detects circular references in the output nodes that are used as inputs
  bool HasCircularReference(vtkMRMLDynamicModelerNode* surfaceEditorNode);

protected:
  vtkSlicerDynamicModelerLogic();
  virtual ~vtkSlicerDynamicModelerLogic();
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;
  void SetMRMLSceneInternal(vtkMRMLScene* newScene) override;

  /// Register MRML Node classes to Scene. Gets called automatically when the MRMLScene is attached to this logic class.
  void RegisterNodes() override;

  void OnMRMLSceneNodeAdded(vtkMRMLNode* node) override;
  void OnMRMLSceneNodeRemoved(vtkMRMLNode* node) override;
  void OnMRMLSceneEndImport() override;

  /// Ensures that the vtkSlicerDynamicModelerRule for each rule exists, and is up-to-date.
  void UpdateDynamicModelerRule(vtkMRMLDynamicModelerNode* surfaceEditorNode);

  typedef std::map<std::string, vtkSmartPointer<vtkSlicerDynamicModelerRule> > DynamicModelerRuleList;
  DynamicModelerRuleList Rules;

private:

  vtkSlicerDynamicModelerLogic(const vtkSlicerDynamicModelerLogic&); // Not implemented
  void operator=(const vtkSlicerDynamicModelerLogic&); // Not implemented
};

#endif
