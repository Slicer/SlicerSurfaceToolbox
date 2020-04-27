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

#ifndef __vtkSlicerDynamicModelerRuleFactory_h
#define __vtkSlicerDynamicModelerRuleFactory_h

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <vector>

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

#include "vtkSlicerDynamicModelerRule.h"

class vtkDataObject;

/// \ingroup DynamicModeler
/// \brief Class that can create vtkSlicerDynamicModelerRule instances.
///
/// This singleton class is a repository of all dynamic modelling rules.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerRuleFactory : public vtkObject
{
public:

  vtkTypeMacro(vtkSlicerDynamicModelerRuleFactory, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Registers a new mesh modify rule
  /// Returns true if the rule is successfully registered
  bool RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerRule> rule);

  /// Removes a mesh modify rule from the factory
  /// This does not affect rules that have already been instantiated
  /// Returns true if the rule is successfully unregistered
  bool UnregisterDynamicModelerRuleByClassName(const std::string& ruleClassName);

  /// Get pointer to a new rule, or nullptr if the rule is not registered
  /// Returns nullptr if no matching rule can be found
  vtkSlicerDynamicModelerRule* CreateRuleByClassName(const std::string& ruleClassName);

  /// Get pointer to a new rule, or nullptr if the rule is not registered
  /// Returns nullptr if no matching rule can be found
  vtkSlicerDynamicModelerRule* CreateRuleByName(const std::string name);

  /// Returns a list of all registered rules
  const std::vector<std::string> GetDynamicModelerRuleClassNames();

  /// Returns a list of all registered rules
  const std::vector<std::string> GetDynamicModelerRuleNames();

public:
  /// Return the singleton instance with no reference counting.
  static vtkSlicerDynamicModelerRuleFactory* GetInstance();

  /// This is a singleton pattern New.  There will only be ONE
  /// reference to a vtkSlicerDynamicModelerRuleFactory object per process.  Clients that
  /// call this must call Delete on the object so that the reference
  /// counting will work. The single instance will be unreferenced when
  /// the program exits.
  static vtkSlicerDynamicModelerRuleFactory* New();

protected:
  vtkSlicerDynamicModelerRuleFactory();
  ~vtkSlicerDynamicModelerRuleFactory() override;
  vtkSlicerDynamicModelerRuleFactory(const vtkSlicerDynamicModelerRuleFactory&);
  void operator=(const vtkSlicerDynamicModelerRuleFactory&);

  friend class vtkSlicerDynamicModelerRuleFactoryInitialize;
  typedef vtkSlicerDynamicModelerRuleFactory Self;

  // Singleton management functions.
  static void classInitialize();
  static void classFinalize();

  /// Registered rule classes
  std::vector< vtkSmartPointer<vtkSlicerDynamicModelerRule> > RegisteredRules;
};


/// Utility class to make sure qSlicerModuleManager is initialized before it is used.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerRuleFactoryInitialize
{
public:
  typedef vtkSlicerDynamicModelerRuleFactoryInitialize Self;

  vtkSlicerDynamicModelerRuleFactoryInitialize();
  ~vtkSlicerDynamicModelerRuleFactoryInitialize();

private:
  static unsigned int Count;
};

/// This instance will show up in any translation unit that uses
/// vtkSlicerDynamicModelerRuleFactory.  It will make sure vtkSlicerDynamicModelerRuleFactory is initialized
/// before it is used.
static vtkSlicerDynamicModelerRuleFactoryInitialize vtkSlicerDynamicModelerRuleFactoryInitializer;

#endif
