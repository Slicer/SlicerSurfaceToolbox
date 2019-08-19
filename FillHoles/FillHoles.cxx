#include "FillHolesCLP.h"

//VTK Includes
#include "vtkFillHolesFilter.h"
#include "vtkPolyDataNormals.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
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


    vtkNew<vtkFillHolesFilter> fill;

    fill->SetInputData(polyData);
    fill->SetHoleSize(holes);
    fill->Update();


    // Need to auto-orient normals, otherwise holes could appear to be unfilled when only fron-facing elements are chosen to be visible

    vtkNew<vtkPolyDataNormals> normals;

    normals->SetInputData(fill->GetOutput());
    normals->SetAutoOrientNormals(true);
    normals->Update();

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(normals->GetOutput());
    writer->Update();
  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
   return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;

 }
