#include "translateMeshCLP.h"

// VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkNew.h"
#include "vtkTransformFilter.h"
#include "vtkTransform.h"
#include "vtkPoints.h"



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

    vtkSmartPointer<vtkPoints> PointsVTK;
    PointsVTK = polyData->GetPoints();



    double x[3];

    for( int PointId = 0; PointId < (polyData->GetNumberOfPoints() ); PointId++ )
        {
        PointsVTK->GetPoint(PointId, x);
        double vert[3];

        vert[0] = x[0] + dimX;
        vert[1] = x[1] + dimY;
        vert[2] = x[2] + dimZ;

        PointsVTK->SetPoint(PointId, vert);
        }


    //Write to file
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputVolume.c_str());
    writer->SetInputData(polyData);
    writer->Update();
  }
  catch (int e)
   {
     cout << "An exception occurred. Exception Nr. " << e << '\n';
     return EXIT_FAILURE;
   }

  return EXIT_SUCCESS;

 }
