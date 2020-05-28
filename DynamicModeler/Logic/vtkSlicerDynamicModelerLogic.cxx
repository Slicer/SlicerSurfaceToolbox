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
#include "vtkSlicerDynamicModelerToolFactory.h"

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

  this->Tools[surfaceEditorNode->GetID()] = nullptr;
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkCommand::ModifiedEvent);
  events->InsertNextValue(vtkMRMLDynamicModelerNode::InputNodeModifiedEvent);
  vtkObserveMRMLNodeEventsMacro(surfaceEditorNode, events);
  this->UpdateDynamicModelerTool(surfaceEditorNode);
  this->RunDynamicModelerTool(surfaceEditorNode);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  vtkMRMLDynamicModelerNode* surfaceEditorNode = vtkMRMLDynamicModelerNode::SafeDownCast(node);
  if (!surfaceEditorNode)
    {
    return;
    }

  DynamicModelerToolList::iterator tool = this->Tools.find(surfaceEditorNode->GetID());
  if (tool == this->Tools.end())
    {
    return;
    }
  this->Tools.erase(tool);
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

    this->Tools[dynamicModelerNode->GetID()] = nullptr;
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkCommand::ModifiedEvent);
    events->InsertNextValue(vtkMRMLDynamicModelerNode::InputNodeModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(dynamicModelerNode, events);
    this->UpdateDynamicModelerTool(dynamicModelerNode);
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
    this->UpdateDynamicModelerTool(surfaceEditorNode);
    if (surfaceEditorNode->GetContinuousUpdate() && this->HasCircularReference(surfaceEditorNode))
      {
      vtkWarningMacro("Circular reference detected. Disabling continuous update for: " << surfaceEditorNode->GetName());
      surfaceEditorNode->SetContinuousUpdate(false);
      return;
      }
    }

  if (surfaceEditorNode && surfaceEditorNode->GetContinuousUpdate())
    {
    vtkSmartPointer<vtkSlicerDynamicModelerTool> tool = this->GetDynamicModelerTool(surfaceEditorNode);
    if (tool)
      {
      this->RunDynamicModelerTool(surfaceEditorNode);
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
  vtkSmartPointer<vtkSlicerDynamicModelerTool> tool = this->GetDynamicModelerTool(surfaceEditorNode);
  if (!tool)
    {
    return false;
    }

  std::vector<vtkMRMLNode*> inputNodes;
  for (int i = 0; i < tool->GetNumberOfInputNodes(); ++i)
    {
    vtkMRMLNode* inputNode = tool->GetNthInputNode(i, surfaceEditorNode);
    if (inputNode)
      {
      inputNodes.push_back(inputNode);
      }
    }

  for (int i = 0; i < tool->GetNumberOfOutputNodes(); ++i)
    {
    vtkMRMLNode* outputNode = tool->GetNthOutputNode(i, surfaceEditorNode);
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
void vtkSlicerDynamicModelerLogic::UpdateDynamicModelerTool(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return;
    }

  MRMLNodeModifyBlocker blocker(surfaceEditorNode);

  vtkSmartPointer<vtkSlicerDynamicModelerTool> tool = this->GetDynamicModelerTool(surfaceEditorNode);
  if (!tool || strcmp(tool->GetName(), surfaceEditorNode->GetToolName()) != 0)
    {
    // We are changing tool types, and should remove observers to the previous tool
    if (tool)
      {
      for (int i = 0; i < tool->GetNumberOfInputNodes(); ++i)
        {
        std::string referenceRole = tool->GetNthInputNodeReferenceRole(i);
        std::vector<const char*> referenceNodeIds;
        surfaceEditorNode->GetNodeReferenceIDs(referenceRole.c_str(), referenceNodeIds);
        int referenceIndex = 0;
        for (const char* referenceId : referenceNodeIds)
          {
          // Current behavior is to add back references without observers
          // This preserves the selected nodes for each tool
          surfaceEditorNode->SetNthNodeReferenceID(referenceRole.c_str(), referenceIndex, referenceId);
          ++referenceIndex;
          }
        }
      }

    tool = nullptr;
    if (surfaceEditorNode->GetToolName())
      {
      tool = vtkSmartPointer<vtkSlicerDynamicModelerTool>::Take(
        vtkSlicerDynamicModelerToolFactory::GetInstance()->CreateToolByName(surfaceEditorNode->GetToolName()));
      }
    this->Tools[surfaceEditorNode->GetID()] = tool;
    }

  if (tool)
    {
    // Update node observers to ensure that all input nodes are observed
    for (int i = 0; i < tool->GetNumberOfInputNodes(); ++i)
      {
      std::string referenceRole = tool->GetNthInputNodeReferenceRole(i);
      std::vector<const char*> referenceNodeIds;
      surfaceEditorNode->GetNodeReferenceIDs(referenceRole.c_str(), referenceNodeIds);
      vtkIntArray* events = tool->GetNthInputNodeEvents(i);
      int referenceIndex = 0;
      for (const char* referenceId : referenceNodeIds)
        {
        // Current behavior is to add back references without observers
        // This preserves the selected nodes for each tool
        surfaceEditorNode->SetAndObserveNthNodeReferenceID(referenceRole.c_str(), referenceIndex, referenceId, events);
        ++referenceIndex;
        }
      }
    }
}

//---------------------------------------------------------------------------
vtkSlicerDynamicModelerTool* vtkSlicerDynamicModelerLogic::GetDynamicModelerTool(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode || !surfaceEditorNode->GetID())
    {
    return nullptr;
    }

  vtkSmartPointer<vtkSlicerDynamicModelerTool> tool = nullptr;
  DynamicModelerToolList::iterator toolIt = this->Tools.find(surfaceEditorNode->GetID());
  if (toolIt == this->Tools.end())
    {
    return nullptr;
    }
  return toolIt->second;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerLogic::RunDynamicModelerTool(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node!");
    return;
    }
  if (!surfaceEditorNode->GetToolName())
    {
    return;
    }

  vtkSmartPointer<vtkSlicerDynamicModelerTool> tool = this->GetDynamicModelerTool(surfaceEditorNode);
  if (!tool)
    {
    vtkErrorMacro("Could not find tool with name: " << surfaceEditorNode->GetToolName());
    return;
    }
  if (!tool->HasRequiredInputs(surfaceEditorNode))
    {
    return;
    }

  tool->Run(surfaceEditorNode);
}
