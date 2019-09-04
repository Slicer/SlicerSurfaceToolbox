#include "NormalsCLP.h"

//VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkTriangleFilter.h"
#include "vtkSmartPointer.h"
#include "vtkPolyDataNormals.h"
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


    vtkNew<vtkPolyDataNormals> normals;

    normals->SetInputData(polyData);
    normals->SetAutoOrientNormals(orient);
    normals->SetFlipNormals(flip);
    normals->SetSplitting(splitting);
    if(splitting==true){
      // only applicable if splitting is set
      normals->SetFeatureAngle(angle);
    }
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
