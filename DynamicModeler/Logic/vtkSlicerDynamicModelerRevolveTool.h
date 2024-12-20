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

  This file was originally developed by Mauro I. Dominguez.

==============================================================================*/

#ifndef __vtkSlicerDynamicModelerRevolveTool_h
#define __vtkSlicerDynamicModelerRevolveTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkGeneralTransform;
class vtkLinearExtrusionFilter;
class vtkMRMLDynamicModelerNode;
class vtkPolyDataNormals;
class vtkTransformPolyDataFilter;
class vtkTriangleFilter;

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkLinearExtrusionFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modelling tool to revolve an open surface.
///
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerRevolveTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerRevolveTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerRevolveTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the faces selection on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

protected:

  vtkSlicerDynamicModelerRevolveTool();
  ~vtkSlicerDynamicModelerRevolveTool() override;
  void operator=(const vtkSlicerDynamicModelerRevolveTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform> InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkLinearExtrusionFilter> RevolveFilter;
  vtkSmartPointer<vtkTriangleFilter> TriangleFilter;
  vtkSmartPointer<vtkPolyDataNormals> NormalsFilter;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputWorldToModelTransform;

private:
  vtkSlicerDynamicModelerRevolveTool(const vtkSlicerDynamicModelerRevolveTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerRevolveTool_h
