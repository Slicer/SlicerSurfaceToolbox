#include "MC2OriginCLP.h"

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
     //translate center of mesh to origin


    vtkSmartPointer<vtkPolyData> polyData;


    // Read the file
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(inputVolume.c_str());
    reader->Update();
    polyData = reader->GetOutput();

    // sum of original points
    double sum[3];
    sum[0] = 0;
    sum[1] = 0;
    sum[2] = 0;

    for (int i =0; i < polyData->GetNumberOfPoints(); i++)
    {
      double curPoint[3];
      polyData->GetPoint(i, curPoint);
      for(unsigned int dim = 0; dim < 3; dim++)
      {
        sum[dim] += curPoint[dim];
      }
    }

    // Calculate MC
    double MC[3];
    for(unsigned int dim = 0; dim < 3; dim++)
    {
      MC[dim] = (double) sum[dim] / (polyData->GetNumberOfPoints() + 1);
    }
    // create new point set of shifted values
    vtkSmartPointer<vtkPoints> sftpoints;
    sftpoints = polyData->GetPoints();

    for(int pointID = 0; pointID < polyData->GetNumberOfPoints(); pointID++)
    {
      double curPoint[3];
      polyData->GetPoint(pointID, curPoint);
      double sftPoint[3];
      for(unsigned int dim = 0; dim < 3; dim++)
      {
        sftPoint[dim] = curPoint[dim] - MC[dim];
      }
      //shifting the points
      sftpoints->SetPoint(pointID, sftPoint);
    }
    // shift the points
    polyData->SetPoints(sftpoints);



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
