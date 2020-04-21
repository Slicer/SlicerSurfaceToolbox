/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

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

#ifndef __vtkSlicerParametricSurfaceEditorRuleFactory_h
#define __vtkSlicerParametricSurfaceEditorRuleFactory_h

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <vector>

#include "vtkSlicerParametricSurfaceEditorModuleLogicExport.h"

#include "vtkSlicerParametricSurfaceEditorRule.h"

class vtkDataObject;

/// \ingroup ParametricSurfaceEditor
/// \brief Class that can create vtkSlicerParametricSurfaceEditorRule instances.
///
/// This singleton class is a repository of all parametric surface editing rules.
class VTK_SLICER_PARAMETRICSURFACEEDITOR_MODULE_LOGIC_EXPORT vtkSlicerParametricSurfaceEditorRuleFactory : public vtkObject
{
public:

  vtkTypeMacro(vtkSlicerParametricSurfaceEditorRuleFactory, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Registers a new mesh modify rule
  /// Returns true if the rule is successfully registered
  bool RegisterParametricSurfaceEditorRule(vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule);

  /// Removes a mesh modify rule from the factory
  /// This does not affect rules that have already been instantiated
  /// Returns true if the rule is successfully unregistered
  bool UnregisterParametricSurfaceEditorRuleByClassName(const std::string& ruleClassName);

  /// Get pointer to a new rule, or nullptr if the rule is not registered
  /// Returns nullptr if no matching rule can be found
  vtkSlicerParametricSurfaceEditorRule* CreateRuleByClassName(const std::string& ruleClassName);

  /// Get pointer to a new rule, or nullptr if the rule is not registered
  /// Returns nullptr if no matching rule can be found
  vtkSlicerParametricSurfaceEditorRule* CreateRuleByName(const std::string name);

  /// Returns a list of all registered rules
  const std::vector<std::string> GetParametricSurfaceEditorRuleClassNames();

  /// Returns a list of all registered rules
  const std::vector<std::string> GetParametricSurfaceEditorRuleNames();

public:
  /// Return the singleton instance with no reference counting.
  static vtkSlicerParametricSurfaceEditorRuleFactory* GetInstance();

  /// This is a singleton pattern New.  There will only be ONE
  /// reference to a vtkSlicerParametricSurfaceEditorRuleFactory object per process.  Clients that
  /// call this must call Delete on the object so that the reference
  /// counting will work. The single instance will be unreferenced when
  /// the program exits.
  static vtkSlicerParametricSurfaceEditorRuleFactory* New();

protected:
  vtkSlicerParametricSurfaceEditorRuleFactory();
  ~vtkSlicerParametricSurfaceEditorRuleFactory() override;
  vtkSlicerParametricSurfaceEditorRuleFactory(const vtkSlicerParametricSurfaceEditorRuleFactory&);
  void operator=(const vtkSlicerParametricSurfaceEditorRuleFactory&);

  friend class vtkSlicerParametricSurfaceEditorRuleFactoryInitialize;
  typedef vtkSlicerParametricSurfaceEditorRuleFactory Self;

  // Singleton management functions.
  static void classInitialize();
  static void classFinalize();

  /// Registered rule classes
  std::vector< vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> > RegisteredRules;
};


/// Utility class to make sure qSlicerModuleManager is initialized before it is used.
class VTK_SLICER_PARAMETRICSURFACEEDITOR_MODULE_LOGIC_EXPORT vtkSlicerParametricSurfaceEditorRuleFactoryInitialize
{
public:
  typedef vtkSlicerParametricSurfaceEditorRuleFactoryInitialize Self;

  vtkSlicerParametricSurfaceEditorRuleFactoryInitialize();
  ~vtkSlicerParametricSurfaceEditorRuleFactoryInitialize();

private:
  static unsigned int Count;
};

/// This instance will show up in any translation unit that uses
/// vtkSlicerParametricSurfaceEditorRuleFactory.  It will make sure vtkSlicerParametricSurfaceEditorRuleFactory is initialized
/// before it is used.
static vtkSlicerParametricSurfaceEditorRuleFactoryInitialize vtkSlicerParametricSurfaceEditorRuleFactoryInitializer;

#endif
