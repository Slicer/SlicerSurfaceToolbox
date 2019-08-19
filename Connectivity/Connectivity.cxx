#include "ConnectivityCLP.h"

//VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyDataConnectivityFilter.h"
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

    vtkNew<vtkPolyDataConnectivityFilter> connect;

    connect->SetInputData(polyData);
    connect->SetExtractionModeToLargestRegion();
    connect->Update();

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(connect->GetOutput());
    writer->Update();

  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
 }

 return EXIT_SUCCESS;

 }
