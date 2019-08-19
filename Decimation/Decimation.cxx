#include "DecimationCLP.h"

// VTK Includes
#include "vtkDecimatePro.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkTriangleFilter.h"
#include "vtkNew.h"



int main (int argc, char * argv[])
 {
   PARSE_ARGS;

   try{

    vtkSmartPointer<vtkPolyData> inputPolyData;
    vtkSmartPointer<vtkPolyData> polyData;


    // Read the file
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(inputVolume.c_str());
    reader->Update();
    polyData = reader->GetOutput();

    vtkNew<vtkTriangleFilter> triangles;
    triangles->SetInputData(polyData);
    triangles->Update();
    // create poly data with triangle filter
    inputPolyData = triangles->GetOutput();

    vtkNew<vtkDecimatePro> decimate;

    decimate->SetInputData(inputPolyData);
    decimate->SetTargetReduction(Decimate);
    decimate->SetBoundaryVertexDeletion(Boundary);
    decimate->PreserveTopologyOn();
    decimate->Update();

    //Write to file
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(decimate->GetOutput());
    writer->Update();
  }
  catch (int e)
   {
     cout << "An exception occurred. Exception Nr. " << e << '\n';
     return EXIT_FAILURE;
   }

  return EXIT_SUCCESS;

 }
