#include "volumePolyDataCLP.h"

//VTK Includes
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkNew.h"
#include "vtkMassProperties.h"



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

    vtkNew<vtkMassProperties> property;
    property->SetInputData(polyData);
    property->Update();

    double volume = property->GetVolume();

    std::ofstream volumeFile;
    volumeFile.open(outFile.c_str());

    volumeFile << "Volume: " << volume << endl;
    volumeFile.close();

  }
catch (int e)
 {
   cout << "An exception occurred. Exception Nr. " << e << '\n';
   return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;
}
