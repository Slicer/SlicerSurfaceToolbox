/*==============================================================================

  This dynamic modeler tool was developed by Mauro I. Dominguez, Independent
  as Ad-Honorem work.

  Copyright (c) All Rights Reserved.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#include "vtkSlicerDynamicModelerSubdivideTool.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkButterflySubdivisionFilter.h>
#include <vtkLinearSubdivisionFilter.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkTriangleFilter.h>

//----------------------------------------------------------------------------
vtkToolNewMacro(vtkSlicerDynamicModelerSubdivideTool);

const char* SUBDIVIDE_INPUT_MODEL_REFERENCE_ROLE = "Subdivide.InputModel";
const char* SUBDIVIDE_OUTPUT_MODEL_REFERENCE_ROLE = "Subdivide.OutputModel";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerSubdivideTool::vtkSlicerDynamicModelerSubdivideTool()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo inputModel(
    "Model node",
    "Model node to subdivide.",
    inputModelClassNames,
    SUBDIVIDE_INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
  );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Output model (subdivided)",
    "Result from using the selected subdivision filter.",
    inputModelClassNames,
    SUBDIVIDE_OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
  );
  this->OutputNodeInfo.push_back(outputModel);

  /////////
  // Parameters
  ParameterInfo parameterOperationType(
    "Subdivision algorithm",
    "Method used to calculate the new cells of the output mesh.",
    "SubdivisionAlgorithm",
    PARAMETER_STRING_ENUM,
    "Butterfly"
  );

  vtkNew<vtkStringArray> possibleValues;
  parameterOperationType.PossibleValues = possibleValues;
  parameterOperationType.PossibleValues->InsertNextValue("Butterfly");
  parameterOperationType.PossibleValues->InsertNextValue("Linear");
  parameterOperationType.PossibleValues->InsertNextValue("Loop");
  this->InputParameterInfo.push_back(parameterOperationType);

  ParameterInfo parameterNumberOfIterations(
    "Number of iterations",
    "Number of times the subdivision algorithm is applied. If 0, the input mesh is only triangulated.",
    "NumberOfIterations",
    PARAMETER_INT,
    1
  );

  vtkNew<vtkDoubleArray> numberOfIterationsRange;
  numberOfIterationsRange->SetNumberOfComponents(1);
  numberOfIterationsRange->SetNumberOfValues(2);
  numberOfIterationsRange->SetValue(0, 0);
  numberOfIterationsRange->SetValue(1, 20);
  parameterNumberOfIterations.NumbersRange = numberOfIterationsRange;
  
  this->InputParameterInfo.push_back(parameterNumberOfIterations);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->AuxiliarTriangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->AuxiliarTriangleFilter->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());

  this->ButterflySubdivisionFilter = vtkSmartPointer<vtkButterflySubdivisionFilter>::New();
  this->LinearSubdivisionFilter = vtkSmartPointer<vtkLinearSubdivisionFilter>::New();
  this->LoopSubdivisionFilter = vtkSmartPointer<vtkLoopSubdivisionFilter>::New();

  this->OutputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputModelToWorldTransformFilter->SetTransform(this->OutputWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerSubdivideTool::~vtkSlicerDynamicModelerSubdivideTool()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerSubdivideTool::GetName()
{
  return "Subdivide";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerSubdivideTool::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SUBDIVIDE_OUTPUT_MODEL_REFERENCE_ROLE));
  if (!outputModelNode)
    {
    // Nothing to output.
    return true;
    }
  
  int numberOfIterations = this->GetNthInputParameterValue(1, surfaceEditorNode).ToInt();
  if (numberOfIterations >= 1)
    {
    this->ButterflySubdivisionFilter->SetNumberOfSubdivisions(numberOfIterations);
    this->LinearSubdivisionFilter->SetNumberOfSubdivisions(numberOfIterations);
    this->LoopSubdivisionFilter->SetNumberOfSubdivisions(numberOfIterations);

    std::string subdivisionAlgorithm = this->GetNthInputParameterValue(0, surfaceEditorNode).ToString();
    if (subdivisionAlgorithm == "Butterfly")
      {
      this->OutputModelToWorldTransformFilter->SetInputConnection(this->ButterflySubdivisionFilter->GetOutputPort());
      }
    else if (subdivisionAlgorithm == "Linear")
      {
      this->OutputModelToWorldTransformFilter->SetInputConnection(this->LinearSubdivisionFilter->GetOutputPort());
      }
    else if (subdivisionAlgorithm == "Loop")
      {
      this->OutputModelToWorldTransformFilter->SetInputConnection(this->LoopSubdivisionFilter->GetOutputPort());
      }
    }
  else
    {
    this->OutputModelToWorldTransformFilter->SetInputConnection(this->AuxiliarTriangleFilter->GetOutputPort());
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(surfaceEditorNode->GetNodeReference(SUBDIVIDE_INPUT_MODEL_REFERENCE_ROLE));
  if (!inputModelNode)
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
    return true;
    }

  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelNodeToWorldTransform);
    }
  else
    {
    this->InputModelNodeToWorldTransform->Identity();
    }
  if (outputModelNode && outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());

  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}
