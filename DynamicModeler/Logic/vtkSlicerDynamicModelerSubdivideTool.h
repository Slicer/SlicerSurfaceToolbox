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

#ifndef __vtkSlicerDynamicModelerSubdivideTool_h
#define __vtkSlicerDynamicModelerSubdivideTool_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <map>
#include <string>
#include <vector>

class vtkDataObject;
class vtkGeneralTransform;
class vtkMRMLDynamicModelerNode;
class vtkButterflySubdivisionFilter;
class vtkLinearSubdivisionFilter;
class vtkLoopSubdivisionFilter;
class vtkTriangleFilter;
class vtkPolyData;

#include "vtkSlicerDynamicModelerTool.h"

/// \brief Dynamic modeler tool to subdivide cells of a mesh.
///
/// Has one node inputs (Surface), and one output (The subdivided surface).
class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModelerSubdivideTool : public vtkSlicerDynamicModelerTool
{
public:
  static vtkSlicerDynamicModelerSubdivideTool* New();
  vtkSlicerDynamicModelerTool* CreateToolInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModelerSubdivideTool, vtkSlicerDynamicModelerTool);

  /// Human-readable name of the mesh modification tool
  const char* GetName() override;

  /// Run the subdivision algorithm on the input model node
  bool RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode) override;

protected:
  vtkSlicerDynamicModelerSubdivideTool();
  ~vtkSlicerDynamicModelerSubdivideTool() override;
  void operator=(const vtkSlicerDynamicModelerSubdivideTool&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter>     InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>            InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkTriangleFilter>              AuxiliarTriangleFilter;

  vtkSmartPointer<vtkButterflySubdivisionFilter>  ButterflySubdivisionFilter;
  vtkSmartPointer<vtkLinearSubdivisionFilter>     LinearSubdivisionFilter;
  vtkSmartPointer<vtkLoopSubdivisionFilter>       LoopSubdivisionFilter;

  vtkSmartPointer<vtkTransformPolyDataFilter>    OutputWorldToModelTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>           OutputWorldToModelTransform;

private:
  vtkSlicerDynamicModelerSubdivideTool(const vtkSlicerDynamicModelerSubdivideTool&) = delete;
};

#endif // __vtkSlicerDynamicModelerSubdivideTool_h
