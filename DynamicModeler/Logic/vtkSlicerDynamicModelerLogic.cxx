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
#include "vtkSlicerDynamicModelerLogic.h"
#include "vtkSlicerDynamicModelerRuleFactory.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLDynamicModelerNode.h>

// VTK includes
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cassert>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerDynamicModelerLogic);

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerLogic::vtkSlicerDynamicModelerLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerLogic::~vtkSlicerDynamicModelerLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndImportEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::RegisterNodes()
{
  if (this->GetMRMLScene() == NULL)
    {
    vtkErrorMacro("Scene is invalid");
    return;
    }
  this->GetMRMLScene()->RegisterNodeClass(vtkSmartPointer<vtkMRMLDynamicModelerNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  vtkMRMLDynamicModelerNode* surfaceEditorNode = vtkMRMLDynamicModelerNode::SafeDownCast(node);
  if (!surfaceEditorNode)
    {
    return;
    }
  if (!this->GetMRMLScene() || this->GetMRMLScene()->IsImporting())
    {
    return;
    }

  this->Rules[surfaceEditorNode->GetID()] = nullptr;
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkCommand::ModifiedEvent);
  events->InsertNextValue(vtkMRMLDynamicModelerNode::InputNodeModifiedEvent);
  vtkObserveMRMLNodeEventsMacro(surfaceEditorNode, events);
  this->UpdateDynamicModelerRule(surfaceEditorNode);
  this->RunDynamicModelerRule(surfaceEditorNode);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  vtkMRMLDynamicModelerNode* surfaceEditorNode = vtkMRMLDynamicModelerNode::SafeDownCast(node);
  if (!surfaceEditorNode)
    {
    return;
    }

  DynamicModelerRuleList::iterator rule = this->Rules.find(surfaceEditorNode->GetID());
  if (rule == this->Rules.end())
    {
    return;
    }
  this->Rules.erase(rule);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::OnMRMLSceneEndImport()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  std::vector<vtkMRMLNode*> parametericSurfaceNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLDynamicModelerNode", parametericSurfaceNodes);
  for (vtkMRMLNode* node : parametericSurfaceNodes)
    {
    vtkMRMLDynamicModelerNode* dynamicModelerNode =
      vtkMRMLDynamicModelerNode::SafeDownCast(node);
    if (!dynamicModelerNode)
      {
      continue;
      }

    this->Rules[dynamicModelerNode->GetID()] = nullptr;
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkCommand::ModifiedEvent);
    events->InsertNextValue(vtkMRMLDynamicModelerNode::InputNodeModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(dynamicModelerNode, events);
    this->UpdateDynamicModelerRule(dynamicModelerNode);
    }
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  if (!this->GetMRMLScene() || this->GetMRMLScene()->IsImporting())
    {
    return;
    }

  vtkMRMLDynamicModelerNode* surfaceEditorNode = vtkMRMLDynamicModelerNode::SafeDownCast(caller);
  if (!surfaceEditorNode)
    {
    return;
    }

  if (surfaceEditorNode && event == vtkCommand::ModifiedEvent)
    {
    this->UpdateDynamicModelerRule(surfaceEditorNode);
    if (surfaceEditorNode->GetContinuousUpdate() && this->HasCircularReference(surfaceEditorNode))
      {
      vtkWarningMacro("Circular reference detected. Disabling continuous update for: " << surfaceEditorNode->GetName());
      surfaceEditorNode->SetContinuousUpdate(false);
      return;
      }
    }

  if (surfaceEditorNode && surfaceEditorNode->GetContinuousUpdate())
    {
    vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = this->GetDynamicModelerRule(surfaceEditorNode);
    if (rule)
      {
      this->RunDynamicModelerRule(surfaceEditorNode);
      }
    }
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModelerLogic::HasCircularReference(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return false;
    }
  vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = this->GetDynamicModelerRule(surfaceEditorNode);
  if (!rule)
    {
    return false;
    }

  std::vector<vtkMRMLNode*> inputNodes;
  for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
    {
    vtkMRMLNode* inputNode = rule->GetNthInputNode(i, surfaceEditorNode);
    if (inputNode)
      {
      inputNodes.push_back(inputNode);
      }
    }

  for (int i = 0; i < rule->GetNumberOfOutputNodes(); ++i)
    {
    vtkMRMLNode* outputNode = rule->GetNthOutputNode(i, surfaceEditorNode);
    if (!outputNode)
      {
      continue;
      }
    std::vector<vtkMRMLNode*>::iterator inputNodeIt = std::find(inputNodes.begin(), inputNodes.end(), outputNode);
    if (inputNodeIt != inputNodes.end())
      {
      return true;
      }
    }

  return false;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::UpdateDynamicModelerRule(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return;
    }

  MRMLNodeModifyBlocker blocker(surfaceEditorNode);

  vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = this->GetDynamicModelerRule(surfaceEditorNode);
  if (!rule || strcmp(rule->GetName(), surfaceEditorNode->GetRuleName()) != 0)
    {
    // We are changing rule types, and should remove observers to the previous rule
    if (rule)
      {
      for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
        {
        std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
        std::vector<const char*> referenceNodeIds;
        surfaceEditorNode->GetNodeReferenceIDs(referenceRole.c_str(), referenceNodeIds);
        int referenceIndex = 0;
        for (const char* referenceId : referenceNodeIds)
          {
          // Current behavior is to add back references without observers
          // This preserves the selected nodes for each rule
          surfaceEditorNode->SetNthNodeReferenceID(referenceRole.c_str(), referenceIndex, referenceId);
          ++referenceIndex;
          }
        }
      }

    rule = nullptr;
    if (surfaceEditorNode->GetRuleName())
      {
      rule = vtkSmartPointer<vtkSlicerDynamicModelerRule>::Take(
        vtkSlicerDynamicModelerRuleFactory::GetInstance()->CreateRuleByName(surfaceEditorNode->GetRuleName()));
      }
    this->Rules[surfaceEditorNode->GetID()] = rule;
    }

  if (rule)
    {
    // Update node observers to ensure that all input nodes are observed
    for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
      {
      std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
      std::vector<const char*> referenceNodeIds;
      surfaceEditorNode->GetNodeReferenceIDs(referenceRole.c_str(), referenceNodeIds);
      vtkIntArray* events = rule->GetNthInputNodeEvents(i);
      int referenceIndex = 0;
      for (const char* referenceId : referenceNodeIds)
        {
        // Current behavior is to add back references without observers
        // This preserves the selected nodes for each rule
        surfaceEditorNode->SetAndObserveNthNodeReferenceID(referenceRole.c_str(), referenceIndex, referenceId, events);
        ++referenceIndex;
        }
      }
    }
}

//---------------------------------------------------------------------------
vtkSlicerDynamicModelerRule* vtkSlicerDynamicModelerLogic::GetDynamicModelerRule(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = nullptr;
  DynamicModelerRuleList::iterator ruleIt = this->Rules.find(surfaceEditorNode->GetID());
  if (ruleIt == this->Rules.end())
    {
    return nullptr;
    }
  return ruleIt->second;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::RunDynamicModelerRule(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node!");
    return;
    }
  if (!surfaceEditorNode->GetRuleName())
    {
    return;
    }

  vtkSmartPointer<vtkSlicerDynamicModelerRule> rule = this->GetDynamicModelerRule(surfaceEditorNode);
  if (!rule)
    {
    vtkErrorMacro("Could not find rule with name: " << surfaceEditorNode->GetRuleName());
    return;
    }
  if (!rule->HasRequiredInputs(surfaceEditorNode))
    {
    return;
    }

  rule->Run(surfaceEditorNode);
}
