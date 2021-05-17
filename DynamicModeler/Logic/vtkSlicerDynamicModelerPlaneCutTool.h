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

#ifndef __vtkSlicerDynamicModelerPlaneCutTool_h
#define __vtkSlicerDynamicModelerPlaneCutTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <map>
#include <string>
#include <vector>

class vtkClipPolyData;
class vtkDataObject;
class vtkGeneralTransform;
class vtkGeometryFilter;
class vtkImplicitBoolean;
class vtkImplicitFunction;
class vtkMRMLDynamicModelerNode;
class vtkPlane;
class vtkPlaneCollection;
class vtkPolyData;
class vtkThreshold;
class vtkTransformPolyDataFilter;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modelling tool for cutting a single surface mesh with planes
///
/// Has two node inputs (Plane and Surface), and two outputs (Positive/Negative direction surface segments)
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerPlaneCutTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerPlaneCutTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerPlaneCutTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the plane cut on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

  /// Create an end cap on the clipped surface
  static void CreateEndCap(vtkPlaneCollection* planes, vtkPolyData* originalPolyData, vtkImplicitBoolean* cutFunction, vtkPolyData* outputEndCap);

protected:
  vtkSlicerDynamicModelerPlaneCutTool();
  ~vtkSlicerDynamicModelerPlaneCutTool() override;
  void operator=(const vtkSlicerDynamicModelerPlaneCutTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkClipPolyData>            PlaneClipper;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputPositiveWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputPositiveWorldToModelTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputNegativeWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputNegativeWorldToModelTransform;

private:
  vtkSlicerDynamicModelerPlaneCutTool(const vtkSlicerDynamicModelerPlaneCutTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerPlaneCutTool_h
