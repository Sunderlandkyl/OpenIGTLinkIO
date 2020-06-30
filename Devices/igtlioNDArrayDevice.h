#ifndef NDARRAYDEVICE_H
#define NDARRAYDEVICE_H

#include "igtlioDevicesExport.h"

#include "igtlioNDArrayConverter.h"
#include "igtlioDevice.h"

class vtkDataArray;

typedef vtkSmartPointer<class igtlioNDArrayDevice> igtlioNDArrayDevicePointer;

class OPENIGTLINKIO_DEVICES_EXPORT igtlioNDArrayDevice : public igtlioDevice
{
public:
  enum {
    ArrayModifiedEvent         = 118960,
  };

 unsigned int GetDeviceContentModifiedEvent() const;
 std::string GetDeviceType() const;
 int ReceiveIGTLMessage(igtl::MessageBase::Pointer buffer, bool checkCRC) override;
 igtl::MessageBase::Pointer GetIGTLMessage() override;
 igtl::MessageBase::Pointer GetIGTLMessage(MESSAGE_PREFIX prefix);
 std::set<MESSAGE_PREFIX> GetSupportedMessagePrefixes() const override;

  void SetContent(igtlioNDArrayConverter::ContentData content);
  igtlioNDArrayConverter::ContentData GetContent();

 public:
  static igtlioNDArrayDevice *New();
  vtkTypeMacro(igtlioNDArrayDevice,igtlioDevice);

  void PrintSelf(ostream& os, vtkIndent indent);

 protected:
  igtlioNDArrayDevice();
  ~igtlioNDArrayDevice();

 protected:
  igtl::NDArrayMessage::Pointer OutMessage;
  //igtl::GetStatusMessage::Pointer GetMessage;

  igtlioNDArrayConverter::ContentData Content;
};


//---------------------------------------------------------------------------
class OPENIGTLINKIO_DEVICES_EXPORT igtlioNDArrayDeviceCreator : public igtlioDeviceCreator
{
public:
  virtual igtlioDevicePointer Create(std::string device_name);
  std::string GetDeviceType() const override;

  static igtlioNDArrayDeviceCreator *New();
  vtkTypeMacro(igtlioNDArrayDeviceCreator,vtkObject);
};

#endif // NDARRAYDEVICE_H
