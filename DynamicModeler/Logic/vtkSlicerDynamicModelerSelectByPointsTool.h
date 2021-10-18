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

#ifndef __vtkSlicerDynamicModelerSelectByPointsTool_h
#define __vtkSlicerDynamicModelerSelectByPointsTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkFastMarchingGeodesicDistance;
class vtkGeneralTransform;
class vtkMRMLMarkupsFiducialNode;
class vtkPointLocator;
class vtkPolyData;
class vtkTransformPolyDataFilter;
class vtkUnsignedCharArray;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modelling tool to select surface patches on a model using markups fiducials.
///
/// The tool has two input nodes (Surface and Fiducials) and two outputs
/// (surface with "Selection" point scalar values and surface cropped to this selection).
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerSelectByPointsTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerSelectByPointsTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerSelectByPointsTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the faces selection on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

  void CreateOutputDisplayNodes(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;
  
protected:

  // Uses cached locator
  bool UpdateUsingSphereRadius(vtkPolyData* inputMesh_World, vtkMRMLMarkupsFiducialNode* fiducialNode,
    double selectionDistance, bool computeSelectionScalarsModel, bool computeSelectedFacesModel,
    vtkUnsignedCharArray* outputSelectionArray, vtkSmartPointer<vtkPolyData>& selectedFacesMesh_World);

  // Uses cached locator and geodesic distance filter
  bool UpdateUsingGeodesicDistance(vtkPolyData* inputMesh_World, vtkMRMLMarkupsFiducialNode* fiducialNode,
    double selectionDistance, bool computeSelectionScalarsModel, bool computeSelectedFacesModel,
    vtkUnsignedCharArray* outputSelectionArray, vtkSmartPointer<vtkPolyData>& selectedFacesMesh_World);

  vtkSlicerDynamicModelerSelectByPointsTool();
  ~vtkSlicerDynamicModelerSelectByPointsTool() override;
  void operator=(const vtkSlicerDynamicModelerSelectByPointsTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputSelectionScalarsModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputSelectionScalarsModelTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputSelectedFacesModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputSelectedFacesModelTransform;

  // Cache output meshes to minimize need for memory reallocation.
  vtkSmartPointer<vtkPolyData> SelectionScalarsOutputMesh;
  vtkSmartPointer<vtkPolyData> SelectedFacesOutputMesh;

  // Cache filters that are expensive to initialize
  vtkSmartPointer<vtkPointLocator> InputMeshLocator_World;
  vtkSmartPointer<vtkFastMarchingGeodesicDistance> GeodesicDistance;

  // Value is 1 for points that are closer to input fiducials than the selection distance, 0 for others.
  vtkSmartPointer<vtkUnsignedCharArray> SelectionArray;

private:
  vtkSlicerDynamicModelerSelectByPointsTool(const vtkSlicerDynamicModelerSelectByPointsTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerSelectByPointsTool_h
