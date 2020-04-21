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

// ParametricSurfaceEditor includes
#include "vtkSlicerParametricSurfaceEditorRuleFactory.h"
#include "vtkSlicerParametricSurfaceEditorRule.h"
#include "vtkSlicerParametricSurfacePlaneCutRule.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkDataObject.h>

// STD includes
#include <algorithm>

//----------------------------------------------------------------------------
// The compression rule manager singleton.
// This MUST be default initialized to zero by the compiler and is
// therefore not initialized here.  The ClassInitialize and ClassFinalize methods handle this instance.
static vtkSlicerParametricSurfaceEditorRuleFactory* vtkSlicerParametricSurfaceEditorRuleFactoryInstance;


//----------------------------------------------------------------------------
// Must NOT be initialized.  Default initialization to zero is necessary.
unsigned int vtkSlicerParametricSurfaceEditorRuleFactoryInitialize::Count;

//----------------------------------------------------------------------------
// Implementation of vtkSlicerParametricSurfaceEditorRuleFactoryInitialize class.
//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRuleFactoryInitialize::vtkSlicerParametricSurfaceEditorRuleFactoryInitialize()
{
  if (++Self::Count == 1)
    {
    vtkSlicerParametricSurfaceEditorRuleFactory::classInitialize();
    }
}

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRuleFactoryInitialize::~vtkSlicerParametricSurfaceEditorRuleFactoryInitialize()
{
  if (--Self::Count == 0)
    {
    vtkSlicerParametricSurfaceEditorRuleFactory::classFinalize();
    }
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// Up the reference count so it behaves like New
vtkSlicerParametricSurfaceEditorRuleFactory* vtkSlicerParametricSurfaceEditorRuleFactory::New()
{
  vtkSlicerParametricSurfaceEditorRuleFactory* ret = vtkSlicerParametricSurfaceEditorRuleFactory::GetInstance();
  ret->Register(nullptr);
  return ret;
}

//----------------------------------------------------------------------------
// Return the single instance of the vtkSlicerParametricSurfaceEditorRuleFactory
vtkSlicerParametricSurfaceEditorRuleFactory* vtkSlicerParametricSurfaceEditorRuleFactory::GetInstance()
{
  if (!vtkSlicerParametricSurfaceEditorRuleFactoryInstance)
    {
    // Try the factory first
    vtkSlicerParametricSurfaceEditorRuleFactoryInstance = (vtkSlicerParametricSurfaceEditorRuleFactory*)vtkObjectFactory::CreateInstance("vtkSlicerParametricSurfaceEditorRuleFactory");
    // if the factory did not provide one, then create it here
    if (!vtkSlicerParametricSurfaceEditorRuleFactoryInstance)
      {
      vtkSlicerParametricSurfaceEditorRuleFactoryInstance = new vtkSlicerParametricSurfaceEditorRuleFactory;
#ifdef VTK_HAS_INITIALIZE_OBJECT_BASE
      vtkSlicerParametricSurfaceEditorRuleFactoryInstance->InitializeObjectBase();
#endif
      }
    }
  // return the instance
  return vtkSlicerParametricSurfaceEditorRuleFactoryInstance;
}

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRuleFactory::vtkSlicerParametricSurfaceEditorRuleFactory()
= default;

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRuleFactory::~vtkSlicerParametricSurfaceEditorRuleFactory()
= default;

//----------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorRuleFactory::PrintSelf(ostream & os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorRuleFactory::classInitialize()
{
  // Allocate the singleton
  vtkSlicerParametricSurfaceEditorRuleFactoryInstance = vtkSlicerParametricSurfaceEditorRuleFactory::GetInstance();

  vtkSlicerParametricSurfaceEditorRuleFactoryInstance->RegisterParametricSurfaceEditorRule(vtkSmartPointer<vtkSlicerParametricSurfacePlaneCutRule>::New());
}

//----------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorRuleFactory::classFinalize()
{
  vtkSlicerParametricSurfaceEditorRuleFactoryInstance->Delete();
  vtkSlicerParametricSurfaceEditorRuleFactoryInstance = nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerParametricSurfaceEditorRuleFactory::RegisterParametricSurfaceEditorRule(vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule)
{
  for (unsigned int i = 0; i < this->RegisteredRules.size(); ++i)
    {
    if (strcmp(this->RegisteredRules[i]->GetClassName(), rule->GetClassName()) == 0)
      {
      vtkWarningMacro("RegisterStreamingCodec failed: rule is already registered");
      return false;
      }
    }
  this->RegisteredRules.push_back(rule);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerParametricSurfaceEditorRuleFactory::UnregisterParametricSurfaceEditorRuleByClassName(const std::string & className)
{
  std::vector<vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = *ruleIt;
    if (strcmp(rule->GetClassName(), className.c_str()) == 0)
      {
      this->RegisteredRules.erase(ruleIt);
      return true;
      }
    }
  vtkWarningMacro("UnRegisterStreamingCodecByClassName failed: rule not found");
  return false;
}

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRule* vtkSlicerParametricSurfaceEditorRuleFactory::CreateRuleByClassName(const std::string & className)
{
  std::vector<vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = *ruleIt;
    if (strcmp(rule->GetClassName(), className.c_str()) == 0)
      {
      return rule->CreateRuleInstance();
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRule* vtkSlicerParametricSurfaceEditorRuleFactory::CreateRuleByName(const std::string name)
{
  std::vector<vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = *ruleIt;
    if (rule->GetName() == name)
      {
      return rule->CreateRuleInstance();
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
const std::vector<std::string> vtkSlicerParametricSurfaceEditorRuleFactory::GetParametricSurfaceEditorRuleClassNames()
{
  std::vector<std::string> ruleClassNames;
  std::vector<vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = *ruleIt;
    ruleClassNames.emplace_back(rule->GetClassName());
    }
  return ruleClassNames;
}

//----------------------------------------------------------------------------
const std::vector<std::string> vtkSlicerParametricSurfaceEditorRuleFactory::GetParametricSurfaceEditorRuleNames()
{
  std::vector<std::string> names;
  std::vector<vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = *ruleIt;
    names.push_back(rule->GetName());
    }
  return names;
}
