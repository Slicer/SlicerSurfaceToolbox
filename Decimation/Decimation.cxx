#include "DecimationCLP.h"

// VTK Includes
#include "vtkDecimatePro.h"
#include "vtkNew.h"
#include "vtkOBJReader.h"
#include "vtkOBJWriter.h"
#include "vtkPolyData.h"
#include "vtkQuadricDecimation.h"
#include "vtkSmartPointer.h"
#include "vtkTriangleFilter.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include <vtksys/SystemTools.hxx>

#include "Simplify.h" // FastQuadric method

int main(int argc, char* argv[])
{
  PARSE_ARGS;

  std::string inputModelExt = vtksys::SystemTools::LowerCase(vtksys::SystemTools::GetFilenameLastExtension(inputModel));
  std::string outputModelExt = vtksys::SystemTools::LowerCase(vtksys::SystemTools::GetFilenameLastExtension(outputModel));

  if (method == "FastQuadric")
    {
    if (inputModelExt != ".obj" || outputModelExt != ".obj")
      {
      std::cerr << "FastQuadric method only supports input/output mesh files in OBJ format." << std::endl;
      return EXIT_FAILURE;
      }
    Simplify::load_obj(inputModel.c_str());
    if ((Simplify::triangles.size() < 3) || (Simplify::vertices.size() < 3))
      {
      std::cerr << "Minimum 3 triangles are needed." << std::endl;
      return EXIT_FAILURE;
      }
    int target_count = round((float)Simplify::triangles.size() * (1.0-reductionFactor));
    if (target_count < 4)
      {
      std::cerr << "Object will not survive such extreme decimation." << std::endl;
      return EXIT_FAILURE;
      }
    std::cout << "Input: " << Simplify::vertices.size() << " vertices,"
      << Simplify::triangles.size() << " triangles (target " << target_count << ")" << std::endl;
    size_t startSize = Simplify::triangles.size();
    if (lossless)
      {
      Simplify::simplify_mesh_lossless(verbose);
      }
    else
      {
      Simplify::simplify_mesh(target_count, aggressiveness, verbose);
      }
    if (Simplify::triangles.size() >= startSize)
      {
      std::cerr << "Unable to reduce mesh." << std::endl;
      return EXIT_FAILURE;
      }
    Simplify::write_obj(outputModel.c_str());
    double achievedReduction = 1.0 - (double)Simplify::triangles.size() / (double)startSize;
    std::cout << "Output: " << Simplify::vertices.size() << " vertices,"
      << Simplify::triangles.size() << " triangles (" << achievedReduction << " reduction)" << std::endl;
    return EXIT_SUCCESS;
    }

  // VTK decimation filters

  // Read the input model
  vtkSmartPointer<vtkPolyData> inputPolyData;
  if (inputModelExt == ".obj")
    {
    vtkNew<vtkOBJReader> reader;
    reader->SetFileName(inputModel.c_str());
    reader->Update();
    inputPolyData = reader->GetOutput();
    }
  else if (inputModelExt == ".vtp")
    {
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(inputModel.c_str());
    reader->Update();
    inputPolyData = reader->GetOutput();
    }
  else
    {
    std::cerr << "Input mesh is expected in OBJ or VTP file format." << std::endl;
    return EXIT_FAILURE;
    }

  // Triangulate input mesh
  vtkNew<vtkTriangleFilter> triangles;
  triangles->SetInputData(inputPolyData);
  triangles->Update();
  inputPolyData = triangles->GetOutput();

  vtkSmartPointer<vtkPolyData> outputPolyData;
  if (method == "Quadric")
    {
    vtkNew<vtkQuadricDecimation> decimate;
    decimate->SetInputData(inputPolyData);
    decimate->SetTargetReduction(reductionFactor);
    //decimate->SetVolumePreservation(true);
    decimate->Update();
    outputPolyData = decimate->GetOutput();
    }
  else
    {
    vtkNew<vtkDecimatePro> decimate;
    decimate->SetInputData(inputPolyData);
    decimate->SetTargetReduction(reductionFactor);
    decimate->SetBoundaryVertexDeletion(boundaryDeletion);
    decimate->PreserveTopologyOn();
    decimate->Update();
    outputPolyData = decimate->GetOutput();
    }

  //Write to file
  if (outputModelExt == ".obj")
    {
    vtkNew<vtkOBJWriter> writer;
    writer->SetFileName(outputModel.c_str());
    writer->SetInputData(outputPolyData);
    writer->Update();
    }
  else if (outputModelExt == ".vtp")
    {
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(outputModel.c_str());
    writer->SetInputData(outputPolyData);
    writer->Update();
    }
  else
    {
    std::cerr << "Output mesh can be written in OBJ or VTP file format." << std::endl;
    return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
