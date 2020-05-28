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

#ifndef __vtkSlicerDynamicModelerToolFactory_h
#define __vtkSlicerDynamicModelerToolFactory_h

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <vector>

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

#include "vtkSlicerDynamicModelerTool.h"

class vtkDataObject;

/// \ingroup DynamicModeler
/// \brief Class that can create vtkSlicerDynamicModelerTool instances.
///
/// This singleton class is a repository of all dynamic modelling tools.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerToolFactory : public vtkObject
{
public:

  vtkTypeMacro(vtkSlicerDynamicModelerToolFactory, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Registers a new mesh modify tool
  /// Returns true if the tool is successfully registered
  bool RegisterDynamicModelerTool(vtkSmartPointer<vtkSlicerDynamicModelerTool> tool);

  /// Removes a mesh modify tool from the factory
  /// This does not affect tools that have already been instantiated
  /// Returns true if the tool is successfully unregistered
  bool UnregisterDynamicModelerToolByClassName(const std::string& toolClassName);

  /// Get pointer to a new tool, or nullptr if the tool is not registered
  /// Returns nullptr if no matching tool can be found
  vtkSlicerDynamicModelerTool* CreateToolByClassName(const std::string& toolClassName);

  /// Get pointer to a new tool, or nullptr if the tool is not registered
  /// Returns nullptr if no matching tool can be found
  vtkSlicerDynamicModelerTool* CreateToolByName(const std::string name);

  /// Returns a list of all registered tools
  const std::vector<std::string> GetDynamicModelerToolClassNames();

  /// Returns a list of all registered tools
  const std::vector<std::string> GetDynamicModelerToolNames();

public:
  /// Return the singleton instance with no reference counting.
  static vtkSlicerDynamicModelerToolFactory* GetInstance();

  /// This is a singleton pattern New.  There will only be ONE
  /// reference to a vtkSlicerDynamicModelerToolFactory object per process.  Clients that
  /// call this must call Delete on the object so that the reference
  /// counting will work. The single instance will be unreferenced when
  /// the program exits.
  static vtkSlicerDynamicModelerToolFactory* New();

protected:
  vtkSlicerDynamicModelerToolFactory();
  ~vtkSlicerDynamicModelerToolFactory() override;
  vtkSlicerDynamicModelerToolFactory(const vtkSlicerDynamicModelerToolFactory&);
  void operator=(const vtkSlicerDynamicModelerToolFactory&);

  friend class vtkSlicerDynamicModelerToolFactoryInitialize;
  typedef vtkSlicerDynamicModelerToolFactory Self;

  // Singleton management functions.
  static void classInitialize();
  static void classFinalize();

  /// Registered tool classes
  std::vector< vtkSmartPointer<vtkSlicerDynamicModelerTool> > RegisteredTools;
};


/// Utility class to make sure qSlicerModuleManager is initialized before it is used.
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerToolFactoryInitialize
{
public:
  typedef vtkSlicerDynamicModelerToolFactoryInitialize Self;

  vtkSlicerDynamicModelerToolFactoryInitialize();
  ~vtkSlicerDynamicModelerToolFactoryInitialize();

private:
  static unsigned int Count;
};

/// This instance will show up in any translation unit that uses
/// vtkSlicerDynamicModelerToolFactory.  It will make sure vtkSlicerDynamicModelerToolFactory is initialized
/// before it is used.
static vtkSlicerDynamicModelerToolFactoryInitialize vtkSlicerDynamicModelerToolFactoryInitializer;

#endif
