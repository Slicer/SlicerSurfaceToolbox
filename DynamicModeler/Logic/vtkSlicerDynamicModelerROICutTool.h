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

#ifndef __vtkSlicerDynamicModelerROICutTool_h
#define __vtkSlicerDynamicModelerROICutTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkClipPolyData;
class vtkGeneralTransform;
class vtkMRMLDynamicModelerNode;
class vtkTransformPolyDataFilter;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modeler tool for cutting a single surface mesh with ROIs.
///
/// Has two node inputs (ROI and Surface), and two outputs (Positive/Negative direction surface segments).
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerROICutTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerROICutTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerROICutTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the ROI cut on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* dynamicModelerNode) override;


  // Inputs

  /// Reference role used for the input model (vtkMRMLModelNode)
  static const char* GetInputModelReferenceRole();
  /// Reference role used for the input ROI (vtkMRMLMarkupsROINode)
  static const char* GetInputROIReferenceRole();


  // Outputs

  /// Reference role used for the output model that is inside the ROI (vtkMRMLModelNode)
  static const char* GetOutputInsideModelReferenceRole();
  /// Reference role used for the output model that is outside the ROI (vtkMRMLModelNode)
  static const char* GetOutputOutsideModelReferenceRole();


  // Parameters

    /// Node attribute that is used to indicate if the output models should be capped (vtkMRMLModelNode)
  static const char* GetCapSurfaceAttributeName();

protected:
  vtkSlicerDynamicModelerROICutTool();
  ~vtkSlicerDynamicModelerROICutTool() override;
  void operator=(const vtkSlicerDynamicModelerROICutTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkClipPolyData>            ROIClipper;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputInsideWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputInsideWorldToModelTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputOutsideWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputOutsideWorldToModelTransform;

private:
  vtkSlicerDynamicModelerROICutTool(const vtkSlicerDynamicModelerROICutTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerROICutTool_h
