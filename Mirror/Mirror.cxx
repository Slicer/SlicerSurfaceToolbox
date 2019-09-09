#include "MirrorCLP.h"

//VTK Includes
#include "vtkTransformPolyDataFilter.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkTriangleFilter.h"
#include "vtkReverseSense.h"
#include "vtkSmartPointer.h"
#include "vtkTransform.h"
#include "vtkNew.h"


int main (int argc, char * argv[])
 {
 PARSE_ARGS;

 try{
    // Read the file
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(inputVolume.c_str());
    reader->Update();
    vtkSmartPointer<vtkPolyData> polyData = reader->GetOutput();

    vtkNew<vtkMatrix4x4> transformMatrix;
    transformMatrix->SetElement(0, 0, xAxis ? -1 : 1);
    transformMatrix->SetElement(1, 1, yAxis ? -1 : 1);
    transformMatrix->SetElement(2, 2, zAxis ? -1 : 1);

    vtkNew<vtkTransform> transform;
    transform->SetMatrix(transformMatrix);

    vtkNew<vtkTransformPolyDataFilter> transformFilter;
    transformFilter->SetInputData(polyData);
    transformFilter->SetTransform(transform);
    transformFilter->Update();
    vtkSmartPointer<vtkPolyData> surface = transformFilter->GetOutput();

    if (transformMatrix->Determinant() < 0)
      {
      vtkNew<vtkReverseSense> reverse;
      reverse->SetInputData(surface);
      reverse->Update();
      surface = reverse->GetOutput();
      }

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(surface);
    writer->Update();
  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
   return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;

 }
