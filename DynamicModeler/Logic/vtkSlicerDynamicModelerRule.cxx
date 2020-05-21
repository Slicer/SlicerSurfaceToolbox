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

#include "vtkSlicerDynamicModelerRule.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

/// DynamicModeler MRML includes
#include "vtkMRMLDynamicModelerNode.h"

/// MRML includes
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLDisplayNode.h>

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRule::vtkSlicerDynamicModelerRule()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRule::~vtkSlicerDynamicModelerRule()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerRule* vtkSlicerDynamicModelerRule::Clone()
{
  vtkSlicerDynamicModelerRule* clone = this->CreateRuleInstance();
  return clone;
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModelerRule::GetNumberOfInputNodes()
{
  return this->InputNodeInfo.size();
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModelerRule::GetNumberOfInputParameters()
{
  return this->InputParameterInfo.size();
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModelerRule::GetNumberOfOutputNodes()
{
  return this->OutputNodeInfo.size();
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthInputNodeName(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return "";
    }
  return this->InputNodeInfo[n].Name;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthInputNodeDescription(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return "";
    }
  return this->InputNodeInfo[n].Description;
}

//----------------------------------------------------------------------------
vtkStringArray* vtkSlicerDynamicModelerRule::GetNthInputNodeClassNames(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return nullptr;
    }
  return this->InputNodeInfo[n].ClassNames;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthInputNodeReferenceRole(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return "";
    }
  return this->InputNodeInfo[n].ReferenceRole;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRule::GetNthInputNodeRequired(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return false;
    }
  return this->InputNodeInfo[n].Required;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRule::GetNthInputNodeRepeatable(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return false;
    }
  return this->InputNodeInfo[n].Repeatable;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthOutputNodeName(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return "";
    }
  return this->OutputNodeInfo[n].Name;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthOutputNodeDescription(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return "";
    }
  return this->OutputNodeInfo[n].Description;
}

//----------------------------------------------------------------------------
vtkStringArray* vtkSlicerDynamicModelerRule::GetNthOutputNodeClassNames(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return nullptr;
    }
  return this->OutputNodeInfo[n].ClassNames;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthOutputNodeReferenceRole(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return "";
    }
  return this->OutputNodeInfo[n].ReferenceRole;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRule::GetNthOutputNodeRequired(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return false;
    }
  return this->OutputNodeInfo[n].Required;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerRule::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "Name:\t" << this->GetName() << std::endl;
}

//---------------------------------------------------------------------------
vtkIntArray* vtkSlicerDynamicModelerRule::GetNthInputNodeEvents(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return nullptr;
    }
  return this->InputNodeInfo[n].Events;
}

//---------------------------------------------------------------------------
vtkMRMLNode* vtkSlicerDynamicModelerRule::GetNthInputNode(int n, vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if(!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node");
    return nullptr;
    }
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return nullptr;
    }
  std::string referenceRole = this->GetNthInputNodeReferenceRole(n);
  return surfaceEditorNode->GetNodeReference(referenceRole.c_str());
}

//---------------------------------------------------------------------------
vtkMRMLNode* vtkSlicerDynamicModelerRule::GetNthOutputNode(int n, vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if(!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node");
    return nullptr;
    }
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return nullptr;
    }
  std::string referenceRole = this->GetNthOutputNodeReferenceRole(n);
  return surfaceEditorNode->GetNodeReference(referenceRole.c_str());
}

//---------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthInputParameterName(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return "";
    }
  return this->InputParameterInfo[n].Name;
}

//---------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthInputParameterDescription(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return "";
    }
  return this->InputParameterInfo[n].Description;
}

//---------------------------------------------------------------------------
std::string vtkSlicerDynamicModelerRule::GetNthInputParameterAttributeName(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return "";
    }
  return this->InputParameterInfo[n].AttributeName;
}

//---------------------------------------------------------------------------
int vtkSlicerDynamicModelerRule::GetNthInputParameterType(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return PARAMETER_STRING;
    }
  return this->InputParameterInfo[n].Type;
}

//---------------------------------------------------------------------------
vtkVariant vtkSlicerDynamicModelerRule::GetNthInputParameterValue(int n, vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return PARAMETER_STRING;
    }
  std::string attributeName = this->GetNthInputParameterAttributeName(n);
  const char* parameterValue = surfaceEditorNode->GetAttribute(attributeName.c_str());
  if (!parameterValue)
    {
    return this->InputParameterInfo[n].DefaultValue;
    }
  return vtkVariant(parameterValue);
}

//---------------------------------------------------------------------------
vtkStringArray* vtkSlicerDynamicModelerRule::GetNthInputParameterPossibleValues(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return nullptr;
    }
  return this->InputParameterInfo[n].PossibleValues;
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRule::HasRequiredInputs(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  for (int i = 0; i < this->GetNumberOfInputNodes(); ++i)
    {
    if (!this->GetNthInputNodeRequired(i))
      {
      continue;
      }

    std::string referenceRole = this->GetNthInputNodeReferenceRole(i);
    if (surfaceEditorNode->GetNodeReference(referenceRole.c_str()) == nullptr)
      {
      return false;
      }
    }
  return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRule::HasOutput(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  for (int i = 0; i < this->GetNumberOfOutputNodes(); ++i)
    {
    std::string referenceRole = this->GetNthOutputNodeReferenceRole(i);
    if (surfaceEditorNode->GetNodeReference(referenceRole.c_str()) != nullptr)
      {
      return true;
      }
    }
  return false;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerRule::GetInputNodes(vtkMRMLDynamicModelerNode* surfaceEditorNode, std::vector<vtkMRMLNode*>& inputNodes)
{
  for (int inputIndex = 0; inputIndex < this->GetNumberOfInputNodes(); ++inputIndex)
    {
    std::string referenceRole = this->GetNthInputNodeReferenceRole(inputIndex);
    int numberOfNodeReferences = surfaceEditorNode->GetNumberOfNodeReferences(referenceRole.c_str());
    for (int referenceIndex = 0; referenceIndex < numberOfNodeReferences; ++referenceIndex)
      {
      vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(referenceRole.c_str(), referenceIndex);
      if (inputNode)
        {
        inputNodes.push_back(inputNode);
        }
      }
    }
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerRule::GetOutputNodes(vtkMRMLDynamicModelerNode* surfaceEditorNode, std::vector<vtkMRMLNode*>& outputNodes)
{
  for (int outputIndex = 0; outputIndex < this->GetNumberOfOutputNodes(); ++outputIndex)
    {
    std::string referenceRole = this->GetNthOutputNodeReferenceRole(outputIndex);
    int numberOfNodeReferences = surfaceEditorNode->GetNumberOfNodeReferences(referenceRole.c_str());
    for (int referenceIndex = 0; referenceIndex < numberOfNodeReferences; ++referenceIndex)
      {
      vtkMRMLNode* outputNode = surfaceEditorNode->GetNthNodeReference(referenceRole.c_str(), referenceIndex);
      if (outputNode)
        {
        outputNodes.push_back(outputNode);
        }
      }
    }
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModelerRule::CreateOutputDisplayNodes(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid surfaceEditorNode!");
    return;
    }

  std::vector<vtkMRMLNode*> inputNodes;
  this->GetInputNodes(surfaceEditorNode, inputNodes);

  std::vector<vtkMRMLNode*> outputNodes;
  this->GetOutputNodes(surfaceEditorNode, outputNodes);

  for (vtkMRMLNode* outputNode : outputNodes)
    {
    vtkMRMLDisplayableNode* outputDisplayableNode = vtkMRMLDisplayableNode::SafeDownCast(outputNode);
    if (outputDisplayableNode == nullptr || outputDisplayableNode->GetDisplayNode())
      {
      continue;
      }

    outputDisplayableNode->CreateDefaultDisplayNodes();
    vtkMRMLDisplayNode* outputDisplayNode = outputDisplayableNode->GetDisplayNode();
    if (outputDisplayNode == nullptr)
      {
      continue;
      }

    for (vtkMRMLNode* inputNode : inputNodes)
      {
      vtkMRMLDisplayableNode* inputDisplayableNode = vtkMRMLDisplayableNode::SafeDownCast(inputNode);
      if (inputDisplayableNode == nullptr || !inputDisplayableNode->IsA(outputNode->GetClassName()))
        {
        continue;
        }

      vtkMRMLDisplayNode* inputDisplayNode = inputDisplayableNode->GetDisplayNode();
      if (!inputDisplayNode)
        {
        continue;
        }
      outputDisplayNode->CopyContent(inputDisplayNode);
      break;
      }
    }
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModelerRule::Run(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Input node missing!");
    return false;
    }
  if (!this->HasOutput(surfaceEditorNode))
    {
    vtkErrorMacro("Output node missing!");
    return false;
    }

  this->CreateOutputDisplayNodes(surfaceEditorNode);
  return this->RunInternal(surfaceEditorNode);
}
