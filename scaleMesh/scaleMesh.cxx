#include "scaleMeshCLP.h"

// VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkNew.h"
#include "vtkTransformFilter.h"
#include "vtkTransform.h"



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
    transform->Scale(dimX,dimY,dimZ);

    vtkNew<vtkTransformFilter> scaler;
    scaler->SetInputData(polyData);
    scaler->SetTransform(transform);
    scaler->Update();



    //Write to file
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(scaler->GetOutput());
    writer->Update();
  }
  catch (int e)
   {
     cout << "An exception occurred. Exception Nr. " << e << '\n';
     return EXIT_FAILURE;
   }

  return EXIT_SUCCESS;

 }
