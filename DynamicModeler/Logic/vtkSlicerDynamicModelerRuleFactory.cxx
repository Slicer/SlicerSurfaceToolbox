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

// DynamicModeler includes
#include "vtkSlicerDynamicModelerAppendRule.h"
#include "vtkSlicerDynamicModelerBoundaryCutRule.h"
#include "vtkSlicerDynamicModelerCurveCutRule.h"
#include "vtkSlicerDynamicModelerMirrorRule.h"
#include "vtkSlicerDynamicModelerPlaneCutRule.h"
#include "vtkSlicerDynamicModelerRuleFactory.h"
#include "vtkSlicerDynamicModelerRule.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkDataObject.h>

// STD includes
#include <algorithm>

//----------------------------------------------------------------------------
// The compression rule manager singleton.
// This MUST be default initialized to zero by the compiler and is
// therefore not initialized here.  The ClassInitialize and ClassFinalize methods handle this instance.
static vtkSlicerDynamicModelerRuleFactory* vtkSlicerDynamicModelerRuleFactoryInstance;


//----------------------------------------------------------------------------
// Must NOT be initialized.  Default initialization to zero is necessary.
unsigned int vtkSlicerDynamicModelerRuleFactoryInitialize::Count;

//----------------------------------------------------------------------------
// Implementation of vtkSlicerDynamicModelerRuleFactoryInitialize class.
//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRuleFactoryInitialize::vtkSlicerDynamicModelerRuleFactoryInitialize()
{
  if (++Self::Count == 1)
    {
    vtkSlicerDynamicModelerRuleFactory::classInitialize();
    }
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRuleFactoryInitialize::~vtkSlicerDynamicModelerRuleFactoryInitialize()
{
  if (--Self::Count == 0)
    {
    vtkSlicerDynamicModelerRuleFactory::classFinalize();
    }
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// Up the reference count so it behaves like New
vtkSlicerDynamicModelerRuleFactory* vtkSlicerDynamicModelerRuleFactory::New()
{
  vtkSlicerDynamicModelerRuleFactory* ret = vtkSlicerDynamicModelerRuleFactory::GetInstance();
  ret->Register(nullptr);
  return ret;
}

//----------------------------------------------------------------------------
// Return the single instance of the vtkSlicerDynamicModelerRuleFactory
vtkSlicerDynamicModelerRuleFactory* vtkSlicerDynamicModelerRuleFactory::GetInstance()
{
  if (!vtkSlicerDynamicModelerRuleFactoryInstance)
    {
    // Try the factory first
    vtkSlicerDynamicModelerRuleFactoryInstance = (vtkSlicerDynamicModelerRuleFactory*)vtkObjectFactory::CreateInstance("vtkSlicerDynamicModelerRuleFactory");
    // if the factory did not provide one, then create it here
    if (!vtkSlicerDynamicModelerRuleFactoryInstance)
      {
      vtkSlicerDynamicModelerRuleFactoryInstance = new vtkSlicerDynamicModelerRuleFactory;
#ifdef VTK_HAS_INITIALIZE_OBJECT_BASE
      vtkSlicerDynamicModelerRuleFactoryInstance->InitializeObjectBase();
#endif
      }
    }
  // return the instance
  return vtkSlicerDynamicModelerRuleFactoryInstance;
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRuleFactory::vtkSlicerDynamicModelerRuleFactory()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRuleFactory::~vtkSlicerDynamicModelerRuleFactory()
= default;

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerRuleFactory::PrintSelf(ostream & os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerRuleFactory::classInitialize()
{
  // Allocate the singleton
  vtkSlicerDynamicModelerRuleFactoryInstance = vtkSlicerDynamicModelerRuleFactory::GetInstance();

  vtkSlicerDynamicModelerRuleFactoryInstance->RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerPlaneCutRule>::New());
  vtkSlicerDynamicModelerRuleFactoryInstance->RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerMirrorRule>::New());
  vtkSlicerDynamicModelerRuleFactoryInstance->RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerCurveCutRule>::New());
  vtkSlicerDynamicModelerRuleFactoryInstance->RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerBoundaryCutRule>::New());
  vtkSlicerDynamicModelerRuleFactoryInstance->RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerAppendRule>::New());
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerRuleFactory::classFinalize()
{
  vtkSlicerDynamicModelerRuleFactoryInstance->Delete();
  vtkSlicerDynamicModelerRuleFactoryInstance = nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRuleFactory::RegisterDynamicModelerRule(vtkSmartPointer<vtkSlicerDynamicModelerRule> rule)
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
bool vtkSlicerDynamicModelerRuleFactory::UnregisterDynamicModelerRuleByClassName(const std::string & className)
{
  std::vector<vtkSmartPointer<vtkSlicerDynamicModelerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = *ruleIt;
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
vtkSlicerDynamicModelerRule* vtkSlicerDynamicModelerRuleFactory::CreateRuleByClassName(const std::string & className)
{
  std::vector<vtkSmartPointer<vtkSlicerDynamicModelerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = *ruleIt;
    if (strcmp(rule->GetClassName(), className.c_str()) == 0)
      {
      return rule->CreateRuleInstance();
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRule* vtkSlicerDynamicModelerRuleFactory::CreateRuleByName(const std::string name)
{
  std::vector<vtkSmartPointer<vtkSlicerDynamicModelerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = *ruleIt;
    if (rule->GetName() == name)
      {
      return rule->CreateRuleInstance();
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
const std::vector<std::string> vtkSlicerDynamicModelerRuleFactory::GetDynamicModelerRuleClassNames()
{
  std::vector<std::string> ruleClassNames;
  std::vector<vtkSmartPointer<vtkSlicerDynamicModelerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = *ruleIt;
    ruleClassNames.emplace_back(rule->GetClassName());
    }
  return ruleClassNames;
}

//----------------------------------------------------------------------------
const std::vector<std::string> vtkSlicerDynamicModelerRuleFactory::GetDynamicModelerRuleNames()
{
  std::vector<std::string> names;
  std::vector<vtkSmartPointer<vtkSlicerDynamicModelerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = *ruleIt;
    names.push_back(rule->GetName());
    }
  return names;
}
