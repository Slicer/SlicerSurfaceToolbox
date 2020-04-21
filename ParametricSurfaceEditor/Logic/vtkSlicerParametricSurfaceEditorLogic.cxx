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
#include "vtkSlicerParametricSurfaceEditorLogic.h"
#include "vtkSlicerParametricSurfaceEditorRuleFactory.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLParametricSurfaceEditorNode.h>

// VTK includes
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cassert>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerParametricSurfaceEditorLogic);

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorLogic::vtkSlicerParametricSurfaceEditorLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorLogic::~vtkSlicerParametricSurfaceEditorLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndImportEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic::RegisterNodes()
{
  if (this->GetMRMLScene() == NULL)
    {
    vtkErrorMacro("Scene is invalid");
    return;
    }
  this->GetMRMLScene()->RegisterNodeClass(vtkSmartPointer<vtkMRMLParametricSurfaceEditorNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode = vtkMRMLParametricSurfaceEditorNode::SafeDownCast(node);
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
  events->InsertNextValue(vtkMRMLParametricSurfaceEditorNode::InputNodeModifiedEvent);
  vtkObserveMRMLNodeEventsMacro(surfaceEditorNode, events);
  this->UpdateParametricSurfaceEditorRule(surfaceEditorNode);
  this->RunParametricSurfaceEditorRule(surfaceEditorNode);
}

//---------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode = vtkMRMLParametricSurfaceEditorNode::SafeDownCast(node);
  if (!surfaceEditorNode)
    {
    return;
    }

  ParametricSurfaceEditorRuleList::iterator rule = this->Rules.find(surfaceEditorNode->GetID());
  if (rule == this->Rules.end())
    {
    return;
    }
  this->Rules.erase(rule);
}

//---------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic::OnMRMLSceneEndImport()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  std::vector<vtkMRMLNode*> parametericSurfaceNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLParametricSurfaceEditorNode", parametericSurfaceNodes);
  for (vtkMRMLNode* node : parametericSurfaceNodes)
    {
    vtkMRMLParametricSurfaceEditorNode* parametricSurfaceNode =
      vtkMRMLParametricSurfaceEditorNode::SafeDownCast(node);
    if (!parametricSurfaceNode)
      {
      continue;
      }

    this->Rules[parametricSurfaceNode->GetID()] = nullptr;
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkCommand::ModifiedEvent);
    events->InsertNextValue(vtkMRMLParametricSurfaceEditorNode::InputNodeModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(parametricSurfaceNode, events);
    this->UpdateParametricSurfaceEditorRule(parametricSurfaceNode);
    }
}

//---------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  if (!this->GetMRMLScene() || this->GetMRMLScene()->IsImporting())
    {
    return;
    }

  vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode = vtkMRMLParametricSurfaceEditorNode::SafeDownCast(caller);
  if (!surfaceEditorNode)
    {
    return;
    }

  if (surfaceEditorNode && event == vtkCommand::ModifiedEvent)
    {
    this->UpdateParametricSurfaceEditorRule(surfaceEditorNode);
    if (surfaceEditorNode->GetContinuousUpdate() && this->HasCircularReference(surfaceEditorNode))
      {
      vtkWarningMacro("Circular reference detected. Disabling continuous update for: " << surfaceEditorNode->GetName());
      surfaceEditorNode->SetContinuousUpdate(false);
      return;
      }
    }

  if (surfaceEditorNode && surfaceEditorNode->GetContinuousUpdate())
    {
    vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = this->GetParametricSurfaceEditorRule(surfaceEditorNode);
    if (rule)
      {
      this->RunParametricSurfaceEditorRule(surfaceEditorNode);
      }
    }
}

//---------------------------------------------------------------------------
bool vtkSlicerParametricSurfaceEditorLogic::HasCircularReference(vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return false;
    }
  vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = this->GetParametricSurfaceEditorRule(surfaceEditorNode);
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
void vtkSlicerParametricSurfaceEditorLogic::UpdateParametricSurfaceEditorRule(vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return;
    }

  MRMLNodeModifyBlocker blocker(surfaceEditorNode);

  vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = this->GetParametricSurfaceEditorRule(surfaceEditorNode);
  if (!rule || strcmp(rule->GetName(), surfaceEditorNode->GetRuleName()) != 0)
    {
    // We are changing rule types, and should remove observers to the previous rule
    if (rule)
      {
      for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
        {
        std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
        const char* referenceId = surfaceEditorNode->GetNodeReferenceID(referenceRole.c_str());
        // Current behavior is to add back references without observers
        // This preserves the selected nodes for each rule
        surfaceEditorNode->SetNodeReferenceID(referenceRole.c_str(), referenceId);
        }
      }

    rule = nullptr;
    if (surfaceEditorNode->GetRuleName())
      {
      rule = vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule>::Take(
        vtkSlicerParametricSurfaceEditorRuleFactory::GetInstance()->CreateRuleByName(surfaceEditorNode->GetRuleName()));
      }
    this->Rules[surfaceEditorNode->GetID()] = rule;
    }

  if (rule)
    {
    // Update node observers to ensure that all input nodes are observed
    for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
      {
      std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
      vtkMRMLNode* node = surfaceEditorNode->GetNodeReference(referenceRole.c_str());
      if (!node)
        {
        continue;
        }

      vtkIntArray* events = rule->GetNthInputNodeEvents(i);
      surfaceEditorNode->SetAndObserveNodeReferenceID(referenceRole.c_str(), node->GetID(), events);
      }
    }
}

//---------------------------------------------------------------------------
vtkSlicerParametricSurfaceEditorRule* vtkSlicerParametricSurfaceEditorLogic::GetParametricSurfaceEditorRule(vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode)
{
  vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = nullptr;
  ParametricSurfaceEditorRuleList::iterator ruleIt = this->Rules.find(surfaceEditorNode->GetID());
  if (ruleIt == this->Rules.end())
    {
    return nullptr;
    }
  return ruleIt->second;
}

//---------------------------------------------------------------------------
void vtkSlicerParametricSurfaceEditorLogic::RunParametricSurfaceEditorRule(vtkMRMLParametricSurfaceEditorNode* surfaceEditorNode)
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

  vtkSmartPointer<vtkSlicerParametricSurfaceEditorRule> rule = this->GetParametricSurfaceEditorRule(surfaceEditorNode);
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
