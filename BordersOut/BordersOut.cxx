#include "BordersOutCLP.h"

//VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkTriangleFilter.h"
#include "vtkNew.h"
#include "vtkFeatureEdges.h"
#include "vtkCleanPolyData.h"


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

    vtkNew<vtkFeatureEdges> boundaryEdges;
    boundaryEdges->SetInputData(meshinC->GetOutput());
    boundaryEdges->BoundaryEdgesOn();
    //boundaryEdges->FeatureEdgesOff();
    boundaryEdges->NonManifoldEdgesOff();
    boundaryEdges->ManifoldEdgesOff();
    boundaryEdges->Update();

    //Write to file
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(boundaryEdges->GetOutput());
    writer->Update();
  }
  catch (int e)
   {
     cout << "An exception occurred. Exception Nr. " << e << '\n';
     return EXIT_FAILURE;
   }

  return EXIT_SUCCESS;

 }
