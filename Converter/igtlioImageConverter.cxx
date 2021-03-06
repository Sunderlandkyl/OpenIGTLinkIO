/*==========================================================================

  Portions (c) Copyright 2008-2009 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See Doc/copyright/copyright.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $HeadURL: http://svn.slicer.org/Slicer3/trunk/Modules/OpenIGTLinkIF/vtkIGTLToMRMLImage.cxx $
  Date:      $Date: 2010-12-07 21:39:19 -0500 (Tue, 07 Dec 2010) $
  Version:   $Revision: 15621 $

==========================================================================*/

#include "igtlioImageConverter.h"

#include <igtl_util.h>
#include <igtlImageMessage.h>

#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkVersion.h>

namespace // unnamed namespace
{

//---------------------------------------------------------------------------
// Stream copy + byte swap
//---------------------------------------------------------------------------
int swapCopy16(igtlUint16 * dst, igtlUint16 * src, int n)
{
  igtlUint16 * esrc = src + n;
  while (src < esrc)
    {
    *dst = BYTE_SWAP_INT16(*src);
    dst ++;
    src ++;
    }
  return 1;
}

int swapCopy32(igtlUint32 * dst, igtlUint32 * src, int n)
{
  igtlUint32 * esrc = src + n;
  while (src < esrc)
    {
    *dst = BYTE_SWAP_INT32(*src);
    dst ++;
    src ++;
    }
  return 1;
}

int swapCopy64(igtlUint64 * dst, igtlUint64 * src, int n)
{
  igtlUint64 * esrc = src + n;
  while (src < esrc)
    {
    *dst = BYTE_SWAP_INT64(*src);
    dst ++;
    src ++;
    }
  return 1;
}
} // unnamed namespace


namespace igtlio
{

//---------------------------------------------------------------------------
int ImageConverter::fromIGTL(igtl::MessageBase::Pointer source,
                             HeaderData* header,
                             ContentData* dest,
                             bool checkCRC, igtl::MessageBase::MetaDataMap* metaInfo)
{
  // Create a message buffer to receive image data
  igtl::ImageMessage::Pointer imgMsg;
  imgMsg = igtl::ImageMessage::New();
  imgMsg->Copy(source); // !! TODO: copy makes performance issue.

  // Deserialize the data
  // If CheckCRC==0, CRC check is skipped.
  int c = imgMsg->Unpack(checkCRC);

  if ((c & igtl::MessageHeader::UNPACK_BODY) == 0) // if CRC check fails
    {
    // TODO: error handling
    return 0;
    }

  // get header
  if (!IGTLtoHeader(dynamic_pointer_cast<igtl::MessageBase>(imgMsg), header, metaInfo))
    return 0;

  // get image
  if (IGTLToVTKImageData(imgMsg, dest) == 0)
    return 0;

  // set volume orientation
  if (!dest->transform)
    dest->transform = vtkSmartPointer<vtkMatrix4x4>::New();
  if (IGTLImageToVTKTransform(imgMsg, dest->transform) == 0)
    return 0;

  return 1;
}


//---------------------------------------------------------------------------
int ImageConverter::IGTLToVTKImageData(igtl::ImageMessage::Pointer imgMsg, ContentData *dest)
{
  if (!dest->image)
    dest->image = vtkSmartPointer<vtkImageData>::New();
  vtkSmartPointer<vtkImageData> imageData = dest->image;
  // Retrieve the image data
  int   size[3];          // image dimension
  float spacing[3];       // spacing (mm/pixel)
  int   svsize[3];        // sub-volume size
  int   svoffset[3];      // sub-volume offset
  int   scalarType;       // VTK scalar type
  int   numComponents;    // number of scalar components
  int   endian;

  scalarType = IGTLToVTKScalarType( imgMsg->GetScalarType() );
  endian = imgMsg->GetEndian();
  imgMsg->GetDimensions(size);
  imgMsg->GetSpacing(spacing);
  numComponents = imgMsg->GetNumComponents();
  imgMsg->GetSubVolume(svsize, svoffset);

  // check if the IGTL data fits to the current MRML node  
  int sizeInNode[3]={0,0,0};
  int scalarTypeInNode=VTK_VOID;
  int numComponentsInNode=0;

  if (imageData.GetPointer()!=NULL)
    {
    imageData->GetDimensions(sizeInNode);
    scalarTypeInNode = imageData->GetScalarType();
    numComponentsInNode = imageData->GetNumberOfScalarComponents();
    }
    
  if (imageData.GetPointer()==NULL
      || sizeInNode[0] != size[0] || sizeInNode[1] != size[1] || sizeInNode[2] != size[2]
      || scalarType != scalarTypeInNode
      || numComponentsInNode != numComponents)
    {
    imageData = vtkSmartPointer<vtkImageData>::New();
    dest->image = imageData;
    imageData->SetDimensions(size[0], size[1], size[2]);
    imageData->SetExtent(0, size[0]-1, 0, size[1]-1, 0, size[2]-1);
    imageData->SetOrigin(0.0, 0.0, 0.0);
    imageData->SetSpacing(1.0, 1.0, 1.0);
#if (VTK_MAJOR_VERSION <= 5)
    imageData->SetNumberOfScalarComponents(numComponents);
    imageData->SetScalarType(scalarType);
    imageData->AllocateScalars();
#else
    imageData->AllocateScalars(scalarType, numComponents);
#endif
    }
  
  // Check scalar size
  int scalarSize = imgMsg->GetScalarSize();
  
  int fByteSwap = 0;
  // Check if bytes-swap is required
  if (scalarSize > 1 && 
      ((igtl_is_little_endian() && endian == igtl::ImageMessage::ENDIAN_BIG) ||
       (!igtl_is_little_endian() && endian == igtl::ImageMessage::ENDIAN_LITTLE)))
    {
    // Needs byte swap
    fByteSwap = 1;
    }

  if (imgMsg->GetImageSize() == imgMsg->GetSubVolumeImageSize())
    {
    // In case that volume size == sub-volume size,
    // image is read directly to the memory area of vtkImageData
    // for better performance. 
    if (fByteSwap)
      {
      switch (scalarSize)
        {
        case 2:
          swapCopy16((igtlUint16 *)imageData->GetScalarPointer(),
                     (igtlUint16 *)imgMsg->GetScalarPointer(),
                     imgMsg->GetSubVolumeImageSize() / 2);
          break;
        case 4:
          swapCopy32((igtlUint32 *)imageData->GetScalarPointer(),
                     (igtlUint32 *)imgMsg->GetScalarPointer(),
                     imgMsg->GetSubVolumeImageSize() / 4);
          break;
        case 8:
          swapCopy64((igtlUint64 *)imageData->GetScalarPointer(),
                     (igtlUint64 *)imgMsg->GetScalarPointer(),
                     imgMsg->GetSubVolumeImageSize() / 8);
          break;
        default:
          break;
        }
      }
    else
      {
      memcpy(imageData->GetScalarPointer(),
             imgMsg->GetScalarPointer(), imgMsg->GetSubVolumeImageSize());
      }
    }
  else
    {
    // In case of volume size != sub-volume size,
    // image is loaded into ImageReadBuffer, then copied to
    // the memory area of vtkImageData.
    char* imgPtr = (char*) imageData->GetScalarPointer();
    char* bufPtr = (char*) imgMsg->GetScalarPointer();
    int sizei = size[0];
    int sizej = size[1];
    //int sizek = size[2];
    int subsizei = svsize[0];
    
    int bg_i = svoffset[0];
    //int ed_i = bg_i + svsize[0];
    int bg_j = svoffset[1];
    int ed_j = bg_j + svsize[1];
    int bg_k = svoffset[2];
    int ed_k = bg_k + svsize[2];

    if (fByteSwap)
      {
      switch (scalarSize)
        {
        case 2:
          for (int k = bg_k; k < ed_k; k ++)
            {
            for (int j = bg_j; j < ed_j; j ++)
              {
              swapCopy16((igtlUint16 *)&imgPtr[(sizei*sizej*k + sizei*j + bg_i)*scalarSize],
                         (igtlUint16 *)bufPtr,
                         subsizei);
              bufPtr += subsizei*scalarSize;
              }
            }
          break;
        case 4:
          for (int k = bg_k; k < ed_k; k ++)
            {
            for (int j = bg_j; j < ed_j; j ++)
              {
              swapCopy32((igtlUint32 *)&imgPtr[(sizei*sizej*k + sizei*j + bg_i)*scalarSize],
                         (igtlUint32 *)bufPtr,
                         subsizei);
              bufPtr += subsizei*scalarSize;
              }
            }
          break;
        case 8:
          for (int k = bg_k; k < ed_k; k ++)
            {
            for (int j = bg_j; j < ed_j; j ++)
              {
              swapCopy64((igtlUint64 *)&imgPtr[(sizei*sizej*k + sizei*j + bg_i)*scalarSize],
                         (igtlUint64 *)bufPtr,
                         subsizei);
              bufPtr += subsizei*scalarSize;
              }
            }
          break;
        default:
          break;
        }
      }
    else
      {
      for (int k = bg_k; k < ed_k; k ++)
        {
        for (int j = bg_j; j < ed_j; j ++)
          {
          memcpy(&imgPtr[(sizei*sizej*k + sizei*j + bg_i)*scalarSize],
                 bufPtr, subsizei*scalarSize);
          bufPtr += subsizei*scalarSize;
          }
        }
      }

    }
  
  imageData->Modified();

  return 1;
}

//---------------------------------------------------------------------------
int ImageConverter::IGTLImageToVTKTransform(igtl::ImageMessage::Pointer imgMsg, vtkSmartPointer<vtkMatrix4x4> ijk2ras)
{
  // Retrieve the image data
  int   size[3];          // image dimension
  float spacing[3];       // spacing (mm/pixel)

  imgMsg->GetDimensions(size);
  imgMsg->GetSpacing(spacing);

  igtl::Matrix4x4 matrix; // Image origin and orientation matrix

  imgMsg->GetMatrix(matrix);

  float tx = matrix[0][0];
  float ty = matrix[1][0];
  float tz = matrix[2][0];
  float sx = matrix[0][1];
  float sy = matrix[1][1];
  float sz = matrix[2][1];
  float nx = matrix[0][2];
  float ny = matrix[1][2];
  float nz = matrix[2][2];
  float px = matrix[0][3];
  float py = matrix[1][3];
  float pz = matrix[2][3];

  // normalize
  float psi = sqrt(tx*tx + ty*ty + tz*tz);
  float psj = sqrt(sx*sx + sy*sy + sz*sz);
  float psk = sqrt(nx*nx + ny*ny + nz*nz);
  float ntx = tx / psi;
  float nty = ty / psi;
  float ntz = tz / psi;
  float nsx = sx / psj;
  float nsy = sy / psj;
  float nsz = sz / psj;
  float nnx = nx / psk;
  float nny = ny / psk;
  float nnz = nz / psk;

  // Shift the center
  // NOTE: The center of the image should be shifted due to different
  // definitions of image origin between VTK (Slicer) and OpenIGTLink;
  // OpenIGTLink image has its origin at the center, while VTK image
  // has one at the corner.
  float hfovi = spacing[0] * psi * (size[0]-1) / 2.0;
  float hfovj = spacing[1] * psj * (size[1]-1) / 2.0;
  float hfovk = spacing[2] * psk * (size[2]-1) / 2.0;
  //float hfovk = 0;

  float cx = ntx * hfovi + nsx * hfovj + nnx * hfovk;
  float cy = nty * hfovi + nsy * hfovj + nny * hfovk;
  float cz = ntz * hfovi + nsz * hfovj + nnz * hfovk;
  px = px - cx;
  py = py - cy;
  pz = pz - cz;

  // set volume orientation
  ijk2ras->Identity();
  ijk2ras->Element[0][0] = ntx*spacing[0];
  ijk2ras->Element[1][0] = nty*spacing[0];
  ijk2ras->Element[2][0] = ntz*spacing[0];
  ijk2ras->Element[0][1] = nsx*spacing[1];
  ijk2ras->Element[1][1] = nsy*spacing[1];
  ijk2ras->Element[2][1] = nsz*spacing[1];
  ijk2ras->Element[0][2] = nnx*spacing[2];
  ijk2ras->Element[1][2] = nny*spacing[2];
  ijk2ras->Element[2][2] = nnz*spacing[2];
  ijk2ras->Element[0][3] = px;
  ijk2ras->Element[1][3] = py;
  ijk2ras->Element[2][3] = pz;

  return 1;
}

//----------------------------------------------------------------------------
int ImageConverter::VTKTransformToIGTLImage(const vtkMatrix4x4& ijk2ras, int imageSize[3], double spacing[3], double origin[3], igtl::ImageMessage::Pointer imgMsg)
{
  // VTK: corner image origin
  // OpenIGTLink image message: center image origin

  vtkSmartPointer<vtkMatrix4x4> ijkToVolumeTransform = vtkSmartPointer<vtkMatrix4x4>::New();
  ijkToVolumeTransform->Identity();
  ijkToVolumeTransform->Element[0][0] = spacing[0];
  ijkToVolumeTransform->Element[1][1] = spacing[1];
  ijkToVolumeTransform->Element[2][2] = spacing[2];
  ijkToVolumeTransform->Element[0][3] = origin[0];
  ijkToVolumeTransform->Element[1][3] = origin[1];
  ijkToVolumeTransform->Element[2][3] = origin[2];
  vtkSmartPointer<vtkMatrix4x4> ijkToReferenceTransform = vtkSmartPointer<vtkMatrix4x4>::New();
  vtkMatrix4x4::Multiply4x4(&ijk2ras, ijkToVolumeTransform, ijkToReferenceTransform);

  double ntx = ijkToReferenceTransform->Element[0][0] / spacing[0];
  double nty = ijkToReferenceTransform->Element[1][0] / spacing[0];
  double ntz = ijkToReferenceTransform->Element[2][0] / spacing[0];
  double nsx = ijkToReferenceTransform->Element[0][1] / spacing[1];
  double nsy = ijkToReferenceTransform->Element[1][1] / spacing[1];
  double nsz = ijkToReferenceTransform->Element[2][1] / spacing[1];
  double nnx = ijkToReferenceTransform->Element[0][2] / spacing[2];
  double nny = ijkToReferenceTransform->Element[1][2] / spacing[2];
  double nnz = ijkToReferenceTransform->Element[2][2] / spacing[2];
  double px = ijkToReferenceTransform->Element[0][3];
  double py = ijkToReferenceTransform->Element[1][3];
  double pz = ijkToReferenceTransform->Element[2][3];

  // Shift the center
  // NOTE: The center of the image should be shifted due to different
  // definitions of image origin between VTK (Slicer) and OpenIGTLink;
  // OpenIGTLink image has its origin at the center, while VTK image
  // has one at the corner.

  double hfovi = spacing[0] * (imageSize[0] - 1) / 2.0;
  double hfovj = spacing[1] * (imageSize[1] - 1) / 2.0;
  double hfovk = spacing[2] * (imageSize[2] - 1) / 2.0;

  double cx = ntx * hfovi + nsx * hfovj + nnx * hfovk;
  double cy = nty * hfovi + nsy * hfovj + nny * hfovk;
  double cz = ntz * hfovi + nsz * hfovj + nnz * hfovk;

  px = px + cx;
  py = py + cy;
  pz = pz + cz;

  igtl::Matrix4x4 matrix;
  matrix[0][0] = static_cast<float>(ntx);
  matrix[1][0] = static_cast<float>(nty);
  matrix[2][0] = static_cast<float>(ntz);
  matrix[0][1] = static_cast<float>(nsx);
  matrix[1][1] = static_cast<float>(nsy);
  matrix[2][1] = static_cast<float>(nsz);
  matrix[0][2] = static_cast<float>(nnx);
  matrix[1][2] = static_cast<float>(nny);
  matrix[2][2] = static_cast<float>(nnz);
  matrix[0][3] = static_cast<float>(px);
  matrix[1][3] = static_cast<float>(py);
  matrix[2][3] = static_cast<float>(pz);

  imgMsg->SetMatrix(matrix);

  return 1;
}

//---------------------------------------------------------------------------
int ImageConverter::toIGTL(const HeaderData& header, const ContentData& source, igtl::ImageMessage::Pointer* dest, igtl::MessageBase::MetaDataMap* metaInfo)
{
  if (dest->IsNull())
    *dest = igtl::ImageMessage::New();
  igtl::ImageMessage::Pointer msg = *dest;
  if (metaInfo!=NULL)
    msg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
  igtl::MessageBase::Pointer basemsg = dynamic_pointer_cast<igtl::MessageBase>(msg);
  HeadertoIGTL(header, &basemsg, metaInfo);

  if (source.transform.Get()==NULL)
    std::cerr << "Got NULL input transform" << std::endl;
  if (source.image.Get()==NULL)
    std::cerr << "Got NULL input image" << std::endl;

  vtkSmartPointer<vtkImageData> imageData = source.image;
  int   isize[3];          // image dimension
  //int   svsize[3];        // sub-volume size
  int   scalarType;       // scalar type
  //double *origin;
  double *spacing;       // spacing (mm/pixel)
  int   ncomp;
  int   svoffset[] = {0, 0, 0};           // sub-volume offset
  int   endian;

  scalarType = imageData->GetScalarType();
  ncomp = imageData->GetNumberOfScalarComponents();
  imageData->GetDimensions(isize);
  //imageData->GetExtent(0, isize[0]-1, 0, isize[1]-1, 0, isize[2]-1);
  //origin = imageData->GetOrigin();
  spacing = imageData->GetSpacing();

  // Check endianness of the machine
  endian = igtl::ImageMessage::ENDIAN_BIG;
  if (igtl_is_little_endian())
    {
    endian = igtl::ImageMessage::ENDIAN_LITTLE;
    }

  msg->SetDimensions(isize);
  msg->SetSpacing((float)spacing[0], (float)spacing[1], (float)spacing[2]);
  msg->SetScalarType(scalarType);
  msg->SetEndian(endian);
  msg->SetSubVolume(isize, svoffset);
  msg->SetNumComponents(ncomp);
  msg->AllocateScalars();

  memcpy(msg->GetScalarPointer(),
      imageData->GetScalarPointer(),
      msg->GetImageSize());

  // Transform
  float ntx = source.transform->Element[0][0] / (float)spacing[0];
  float nty = source.transform->Element[1][0] / (float)spacing[0];
  float ntz = source.transform->Element[2][0] / (float)spacing[0];
  float nsx = source.transform->Element[0][1] / (float)spacing[1];
  float nsy = source.transform->Element[1][1] / (float)spacing[1];
  float nsz = source.transform->Element[2][1] / (float)spacing[1];
  float nnx = source.transform->Element[0][2] / (float)spacing[2];
  float nny = source.transform->Element[1][2] / (float)spacing[2];
  float nnz = source.transform->Element[2][2] / (float)spacing[2];
  float px  = source.transform->Element[0][3];
  float py  = source.transform->Element[1][3];
  float pz  = source.transform->Element[2][3];

  // Shift the center
  // NOTE: The center of the image should be shifted due to different
  // definitions of image origin between VTK (Slicer) and OpenIGTLink;
  // OpenIGTLink image has its origin at the center, while VTK image
  // has one at the corner.

  float hfovi = (float)spacing[0] * (float)(isize[0]-1) / 2.0;
  float hfovj = (float)spacing[1] * (float)(isize[1]-1) / 2.0;
  float hfovk = (float)spacing[2] * (float)(isize[2]-1) / 2.0;

  float cx = ntx * hfovi + nsx * hfovj + nnx * hfovk;
  float cy = nty * hfovi + nsy * hfovj + nny * hfovk;
  float cz = ntz * hfovi + nsz * hfovj + nnz * hfovk;

  px = px + cx;
  py = py + cy;
  pz = pz + cz;

  igtl::Matrix4x4 matrix; // Image origin and orientation matrix
  matrix[0][0] = ntx;
  matrix[1][0] = nty;
  matrix[2][0] = ntz;
  matrix[0][1] = nsx;
  matrix[1][1] = nsy;
  matrix[2][1] = nsz;
  matrix[0][2] = nnx;
  matrix[1][2] = nny;
  matrix[2][2] = nnz;
  matrix[0][3] = px;
  matrix[1][3] = py;
  matrix[2][3] = pz;

  msg->SetMatrix(matrix);
  msg->Pack();

  return 1;
}

//---------------------------------------------------------------------------
int ImageConverter::IGTLToVTKScalarType(int igtlType)
{
  switch (igtlType)
    {
    case igtl::ImageMessage::TYPE_INT8: return VTK_CHAR;
    case igtl::ImageMessage::TYPE_UINT8: return VTK_UNSIGNED_CHAR;
    case igtl::ImageMessage::TYPE_INT16: return VTK_SHORT;
    case igtl::ImageMessage::TYPE_UINT16: return VTK_UNSIGNED_SHORT;
    case igtl::ImageMessage::TYPE_INT32: return VTK_UNSIGNED_LONG;
    case igtl::ImageMessage::TYPE_UINT32: return VTK_UNSIGNED_LONG;
    case igtl::ImageMessage::TYPE_FLOAT32: return VTK_FLOAT;
    case igtl::ImageMessage::TYPE_FLOAT64: return VTK_DOUBLE;
    default:
      std::cerr << "Invalid IGTL scalar Type: "<<igtlType << std::endl;
      return VTK_VOID;
    }
}

} //namespace igtlio
