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

#include "vtkImplicitPolyDataPointDistance.h"

// VTK includes
#include <vtkMath.h>
#include <vtkPointLocator.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

vtkStandardNewMacro(vtkImplicitPolyDataPointDistance);

//-----------------------------------------------------------------------------
vtkImplicitPolyDataPointDistance::vtkImplicitPolyDataPointDistance()
{
  this->Input = nullptr;
  this->Locator = vtkSmartPointer<vtkPointLocator>::New();
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::SetInput(vtkPolyData* input)
{
  if ( this->Input != input )
    {
    this->Input = input;

    this->Input->BuildLinks();
    this->NoValue = this->Input->GetLength();

    this->Locator->SetDataSet(this->Input);
    this->Locator->SetTolerance(this->Tolerance);
    this->Locator->SetNumberOfPointsPerBucket(10);
    this->Locator->AutomaticOn();
    this->Locator->BuildLocator();
    }
}

//-----------------------------------------------------------------------------
vtkMTimeType vtkImplicitPolyDataPointDistance::GetMTime()
{
  vtkMTimeType mTime=this->vtkImplicitFunction::GetMTime();
  vtkMTimeType inputMTime;

  if ( this->Input != nullptr )
    {
    inputMTime = this->Input->GetMTime();
    mTime = (inputMTime > mTime ? inputMTime : mTime);
    }

  return mTime;
}

//-----------------------------------------------------------------------------
vtkImplicitPolyDataPointDistance::~vtkImplicitPolyDataPointDistance() = default;

//-----------------------------------------------------------------------------
double vtkImplicitPolyDataPointDistance::EvaluateFunction(double x[3])
{
  if (!this->Locator || !this->Input || this->Input->GetNumberOfPoints() < 1)
    {
    return this->NoValue;
    }

  vtkIdType id = this->Locator->FindClosestPoint(x);
  if (id < 0)
    {
    return this->NoValue;
    }

  double closestPoint[3] = { 0.0 };
  this->Input->GetPoint(id, closestPoint);
  return vtkMath::Distance2BetweenPoints(x, closestPoint);
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::EvaluateGradient(double x[3], double g[3])
{
  if (!this->Locator || !this->Input)
    {
    g[0] = this->NoGradient[0];
    g[1] = this->NoGradient[1];
    g[2] = this->NoGradient[2];
    return;
    }

  vtkIdType id = this->Locator->FindClosestPoint(x);
  double closestPoint[3] = { 0.0 };
  this->Input->GetPoint(id, closestPoint);
  vtkMath::Subtract(x, closestPoint, g);
}


//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkImplicitFunction::PrintSelf(os,indent);
  os << indent << "NoValue: " << this->NoValue << "\n";
  os << indent << "NoGradient: (" << this->NoGradient[0] << ", "
     << this->NoGradient[1] << ", " << this->NoGradient[2] << ")\n";
  os << indent << "Tolerance: " << this->Tolerance << "\n";
  if (this->Input)
    {
    os << indent << "Input : " << this->Input << "\n";
    }
  else
    {
    os << indent << "Input : (none)\n";
    }
}
