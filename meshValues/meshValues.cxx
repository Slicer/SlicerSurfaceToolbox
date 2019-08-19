#include "meshValuesCLP.h"

// VTK Includes
#include "vtkDecimatePro.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkTriangleFilter.h"
#include "vtkNew.h"
#include "vtkCellLocator.h"

//ITK Includes
#include "itkMesh.h"
#include "itkMeshFileReader.h"
#include "itkMeshFileWriter.h"
#include "itkLineCell.h"
#include "itkTriangleCell.h"
#include "itkImageFileWriter.h"
#include "itkResampleImageFilter.h"
#include "itkConstrainedValueAdditionImageFilter.h"
#include "itkBSplineInterpolateImageFunction.h"
#include "itkPluginUtilities.h"
#include <itkDefaultDynamicMeshTraits.h>
#include <itkMetaMeshConverter.h>


int main (int argc, char * argv[])
 {
   PARSE_ARGS;
   try{
     typedef itk::DefaultDynamicMeshTraits<float, 3, 3, float, float> MeshTraitsType;


     typedef MeshTraitsType::PointType                                PointType;

     constexpr unsigned int Dimension = 3;
     using InputPixelType = float;


     using MeshType = itk::Mesh<InputPixelType, Dimension>;
     using  MeshReaderType = itk::MeshFileReader<MeshType>;

     //using MeshWriterType = itk::MeshFileWriter<MeshType>;

     MeshReaderType::Pointer meshReader = MeshReaderType::New();
     meshReader->SetFileName(inputVolume.c_str());
    // meshReader->Update();
     MeshType::Pointer                mesh = meshReader->GetOutput();
     MeshType::PointsContainerPointer Points = mesh->GetPoints();
     int                              numberOfPoints = Points->Size();

     std::ofstream outfileNor;

     outfileNor.open( valFile );
     //NUMBER OF POINTS
     outfileNor << numberOfPoints << "," << std::endl;

     for( unsigned int pointID = 0; pointID < Points->Size(); pointID++ )
      {
      PointType curPoint =  Points->GetElement(pointID);
      // POINTS
      outfileNor << pointID << "," << curPoint << std::endl;
      }

      typedef MeshType::CellsContainer::ConstIterator CellIterator;
      typedef MeshType::CellType                      CellType;
      typedef itk::LineCell<CellType>                 LineType;
      typedef itk::TriangleCell<CellType>             TriangleType;
      // find number&points of cells
      CellIterator cellIterator = mesh->GetCells()->Begin();
      CellIterator cellEnd      = mesh->GetCells()->End();
      PointType    curPoint;
      int num = 0;
      while( cellIterator != cellEnd )
        {
        CellType *                cell = cellIterator.Value();
        TriangleType *            line = dynamic_cast<TriangleType *>(cell);
        LineType::PointIdIterator pit = line->PointIdsBegin();
        int                       pIndex1, pIndex2, pIndex3;

        pIndex1 = *pit;
        ++pit;
        pIndex2 = *pit;
        ++pit;
        pIndex3 = *pit;

        outfileNor << num << "," << pIndex1 << "," << pIndex2 << "," << pIndex3 << "," << std::endl;

        ++cellIterator;
        ++num;
        
        }


     outfileNor.close();

   }
   catch (int e)
    {
      cout << "An exception occurred. Exception Nr. " << e << '\n';
      return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;

}
