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

#ifndef vtkImplicitPolyDataPointDistance_h
#define vtkImplicitPolyDataPointDistance_h

#include "vtkSlicerDynamicModelerModuleLogicExport.h"

// VTK includes
#include "vtkImplicitFunction.h"
#include <vtkPointLocator.h>
#include <vtkPolyData.h>

class VTK_SLICER_DYNAMICMODELER_MODULE_LOGIC_EXPORT vtkImplicitPolyDataPointDistance : public vtkImplicitFunction
{
public:
  static vtkImplicitPolyDataPointDistance *New();
  vtkTypeMacro(vtkImplicitPolyDataPointDistance,vtkImplicitFunction);
  void PrintSelf(ostream& os, vtkIndent indent) override;

   /// Return the MTime also considering the Input dependency.
  vtkMTimeType GetMTime() override;

  /// Evaluate the squared distance to the closest point in the input dataset.
  using vtkImplicitFunction::EvaluateFunction;
  double EvaluateFunction(double x[3]) override;

  /// This function does not currently return the gradient.
  void EvaluateGradient(double x[3], double g[3]) override;

  /// Set the input polydata to use in the locator.
  void SetInput(vtkPolyData *input);

   /// Set/get the function value to use if no input vtkPolyData specified.
  vtkSetMacro(NoValue, double);
  vtkGetMacro(NoValue, double);

  /// Set/get the function gradient to use if no input vtkPolyData
  vtkSetVector3Macro(NoGradient, double);
  vtkGetVector3Macro(NoGradient, double);

  /// Set/get the tolerance usued for the locator.
  vtkGetMacro(Tolerance, double);
  vtkSetMacro(Tolerance, double);

protected:
  vtkImplicitPolyDataPointDistance();
  ~vtkImplicitPolyDataPointDistance() override;

  double NoValue{ 0.0 };
  double NoGradient[3]{ 0.0, 0.0, 0.0 };
  double Tolerance{ 1e-12 };

  vtkSmartPointer<vtkPolyData>     Input;
  vtkSmartPointer<vtkPointLocator> Locator;

private:
  vtkImplicitPolyDataPointDistance(const vtkImplicitPolyDataPointDistance&) = delete;
  void operator=(const vtkImplicitPolyDataPointDistance&) = delete;
};

#endif
