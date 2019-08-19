#include "relaxPolygonsCLP.h"

//VTK Includes
#include "vtkFillHolesFilter.h"
#include "vtkPolyDataNormals.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkNew.h"
#include "vtkCleanPolyData.h"
#include "vtkWindowedSincPolyDataFilter.h"

// Similar Features to Smoothing in surface toolbox, but this is more definite.


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

    vtkNew<vtkCleanPolyData> meshinC;
    meshinC->SetInputData(polyData);
    meshinC->Update();

    vtkNew<vtkWindowedSincPolyDataFilter> smoother;
    smoother->SetInputConnection(meshinC->GetOutputPort());
    smoother->SetNumberOfIterations(Iterations);
    smoother->BoundarySmoothingOff();
    smoother->FeatureEdgeSmoothingOff();
    smoother->SetFeatureAngle(120.0);
    smoother->SetPassBand(0.001);
    smoother->NonManifoldSmoothingOn();
    smoother->NormalizeCoordinatesOn();
    smoother->Update();

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(smoother->GetOutput());
    writer->Update();
  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
   return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;
}
