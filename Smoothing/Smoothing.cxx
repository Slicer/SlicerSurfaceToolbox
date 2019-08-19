#include "SmoothingCLP.h"

//VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkSmoothPolyDataFilter.h"
#include "vtkWindowedSincPolyDataFilter.h"
#include "vtkNew.h"


int main (int argc, char * argv[])
 {
 PARSE_ARGS;

 try{

  vtkSmartPointer<vtkPolyData> polyData;

  vtkNew<vtkXMLPolyDataReader> reader;
  reader->SetFileName(inputVolume.c_str());
  reader->Update();
  polyData = reader->GetOutput();

  // define default filter
  vtkNew<vtkSmoothPolyDataFilter> smoothFilter;

  if(typeFilter=="Laplace"){
    // remain with defuault filter
    ;

  }
  else if(typeFilter=="Taubin"){
    // change filter type
    vtkNew<vtkWindowedSincPolyDataFilter> smoothFilter;

  }
  smoothFilter->SetInputData(polyData);
  smoothFilter->SetNumberOfIterations(Iterations);
  smoothFilter->SetRelaxationFactor(Relaxation);
  smoothFilter->FeatureEdgeSmoothingOff();
  if(Boundary == true){

    smoothFilter->BoundarySmoothingOn();
  }

  smoothFilter->Update();

  vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(smoothFilter->GetOutput());
    writer->Update();
  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
 }

 return EXIT_SUCCESS;

 }
