#include "CleanerCLP.h"

//VTK includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkCleanPolyData.h"
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

    vtkNew<vtkCleanPolyData> cleaner;

    cleaner->SetInputData(polyData);
    cleaner->Update();


    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(cleaner->GetOutput());
    writer->Update();

  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
   return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;

 }
