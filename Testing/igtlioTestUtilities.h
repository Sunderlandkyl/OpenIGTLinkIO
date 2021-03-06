#ifndef TESTUTILITIES_H
#define TESTUTILITIES_H

#include <vtkSmartPointer.h>
#include <vtkMatrix4x4.h>
#include <vtkImageData.h>

#include "igtlioImageDevice.h"
#include "igtlioCommandDevice.h"


#define GenerateErrorIf( condition, errorMessage ) if( condition ) { std::cerr << errorMessage << std::endl; return 1; }
#define TEST_FAILED 1;
#define TEST_SUCCESS 0;

namespace igtlio
{

vtkSmartPointer<vtkMatrix4x4> CreateTestTransform();
vtkSmartPointer<vtkImageData> CreateTestImage();

bool contains(std::vector<int> input, int value, int count);
bool compare(vtkSmartPointer<vtkMatrix4x4> a, vtkSmartPointer<vtkMatrix4x4> b);
bool compare(vtkSmartPointer<vtkImageData> a, vtkSmartPointer<vtkImageData> b);
bool compare(igtlio::ImageDevicePointer a, igtlio::ImageDevicePointer b);
bool compareID(igtlio::CommandDevicePointer a, igtlio::CommandDevicePointer b);
bool compare(igtlio::CommandDevicePointer a, igtlio::CommandDevicePointer b);

std::string boolToString( bool b );



}

#endif // TESTUTILITIES_H
