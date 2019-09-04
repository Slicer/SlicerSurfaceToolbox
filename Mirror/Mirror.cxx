#include "MirrorCLP.h"

//VTK Includes
#include "vtkTransformPolyDataFilter.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkTriangleFilter.h"
#include "vtkSmartPointer.h"
#include "vtkTransform.h"
#include "vtkNew.h"


int main (int argc, char * argv[])
 {
 PARSE_ARGS;

 try{

  vtkSmartPointer<vtkPolyData> polyData;

    // Read the file
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(inputVolume.c_str());
    reader->Update();
    polyData = reader->GetOutput();

    vtkNew<vtkTransform> transform;
    if(xAxis==true){
      transform->RotateWXYZ(180, 1, 0, 0);
    }
    if(yAxis==true){
      transform->RotateWXYZ(180, 0, 1, 0);
    }
    if(zAxis==true){
      transform->RotateWXYZ(180, 0, 0, 1);
    }

    vtkNew<vtkTransformPolyDataFilter> transformFilter;
    transformFilter->SetTransform(transform);
    transformFilter->SetInputData(polyData);
    transformFilter->Update();

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(transformFilter->GetOutput());
    writer->Update();

  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
   return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;

 }
