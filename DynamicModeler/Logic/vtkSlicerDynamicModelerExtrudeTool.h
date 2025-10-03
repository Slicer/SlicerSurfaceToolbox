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

#ifndef __vtkSlicerDynamicModelerExtrudeTool_h
#define __vtkSlicerDynamicModelerExtrudeTool_h

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
#include <vtkAssignAttribute.h>
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
#include <vtkPlaneSource.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modelling tool to extrude an open surface to create a closed surface.
///
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerExtrudeTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerExtrudeTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerExtrudeTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the faces selection on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

  double extrusionLength = 0.0;
  double extrusionScale = 1.0;

protected:

  vtkSlicerDynamicModelerExtrudeTool();
  ~vtkSlicerDynamicModelerExtrudeTool() override;
  void operator=(const vtkSlicerDynamicModelerExtrudeTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputProfileToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform> InputProfileNodeToWorldTransform;

  vtkSmartPointer<vtkPlaneSource> AuxiliarPlaneSource; // used to create a plane for the input profile when it is a markups plane
  vtkSmartPointer<vtkLinearExtrusionFilter> ExtrudeFilter;
  vtkSmartPointer<vtkTriangleFilter> TriangleFilter;
  vtkSmartPointer<vtkPolyDataNormals> NormalsFilter;
  vtkSmartPointer<vtkAssignAttribute> AssignAttributeFilter;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputWorldToModelTransform;

  void setUseNormalsAsExtrusionVector();
  void setUseBestFittingPlaneNormalAsExtrusionVector(vtkMRMLMarkupsNode* markupsNode);
  void setUsePlaneNormalAsExtrusionVector(vtkMRMLMarkupsPlaneNode* markupsPlaneNode);
  void setUseLineAsExtrusionVector(vtkMRMLMarkupsLineNode* markupsLineNode);
  void setUseFiducialAsExtrusionVector(vtkMRMLMarkupsFiducialNode* markupsFiducialNode);

  //void GeneratePolyDataFromMarkups(vtkMRMLMarkupsNode* markupsNode, vtkPolyData* outputPolyData);

private:
  vtkSlicerDynamicModelerExtrudeTool(const vtkSlicerDynamicModelerExtrudeTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerExtrudeTool_h
