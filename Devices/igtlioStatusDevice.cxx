
#include "igtlioStatusDevice.h"
#include <vtkObjectFactory.h>

namespace igtlio
{

//---------------------------------------------------------------------------
DevicePointer StatusDeviceCreator::Create(std::string device_name)
{
 StatusDevicePointer retval = StatusDevicePointer::New();
 retval->SetDeviceName(device_name);
 return retval;
}

//---------------------------------------------------------------------------
std::string StatusDeviceCreator::GetDeviceType() const
{
 return StatusConverter::GetIGTLTypeName();
}

//---------------------------------------------------------------------------
vtkStandardNewMacro(StatusDeviceCreator);




//---------------------------------------------------------------------------
vtkStandardNewMacro(StatusDevice);
//---------------------------------------------------------------------------
StatusDevice::StatusDevice()
{
}

//---------------------------------------------------------------------------
StatusDevice::~StatusDevice()
{
}

//---------------------------------------------------------------------------
unsigned int StatusDevice::GetDeviceContentModifiedEvent() const
{
  return StatusModifiedEvent;
}

//---------------------------------------------------------------------------
std::string StatusDevice::GetDeviceType() const
{
  return StatusConverter::GetIGTLTypeName();
}

//---------------------------------------------------------------------------
int StatusDevice::ReceiveIGTLMessage(igtl::MessageBase::Pointer buffer, bool checkCRC)
{
 if (StatusConverter::fromIGTL(buffer, &HeaderData, &Content, checkCRC, &this->metaInfo))
 {
   this->Modified();
   this->InvokeEvent(StatusModifiedEvent, this);
   this->InvokeEvent(ReceiveEvent);
   return 1;
 }

 return 0;
}

//---------------------------------------------------------------------------
igtl::MessageBase::Pointer StatusDevice::GetIGTLMessage()
{
	/*
 // cannot send a non-existent status (?)
 if (Content.errorname.empty())
  {
  return 0;
  }
  */

 if (!StatusConverter::toIGTL(HeaderData, Content, &this->OutMessage, &this->metaInfo))
   {
   return 0;
   }

 return dynamic_pointer_cast<igtl::MessageBase>(this->OutMessage);
}

//---------------------------------------------------------------------------
igtl::MessageBase::Pointer StatusDevice::GetIGTLMessage(MESSAGE_PREFIX prefix)
{
	/*
 if (prefix==MESSAGE_PREFIX_GET)
  {
   if (this->GetMessage.IsNull())
     {
     this->GetMessage = igtl::GetStatusMessage::New();
     }
   this->GetMessage->SetDeviceName(HeaderData.deviceName.c_str());
   this->GetMessage->Pack();
   return dynamic_pointer_cast<igtl::MessageBase>(this->GetMessage);
  }
  */
 if (prefix==MESSAGE_PREFIX_NOT_DEFINED)
   {
     return this->GetIGTLMessage();
   }

 return igtl::MessageBase::Pointer();
}

//---------------------------------------------------------------------------
std::set<Device::MESSAGE_PREFIX> StatusDevice::GetSupportedMessagePrefixes() const
{
 std::set<MESSAGE_PREFIX> retval;
 retval.insert(MESSAGE_PREFIX_NOT_DEFINED);
 return retval;
}

void StatusDevice::SetContent(StatusConverter::ContentData content)
{
  Content = content;
  this->Modified();
  this->InvokeEvent(StatusModifiedEvent, this);
}

StatusConverter::ContentData StatusDevice::GetContent()
{
  return Content;
}

//---------------------------------------------------------------------------
void StatusDevice::PrintSelf(ostream& os, vtkIndent indent)
{
  Device::PrintSelf(os, indent);

  os << indent << "ErrorCode:\t" << Content.code << "\n";
  os << indent << "ErrorSubCode:\t" << Content.subcode << "\n";
  os << indent << "ErrorName:\t" << Content.errorname << "\n";
  os << indent << "StatusString:\t" << Content.statusstring << "\n";
}

} //namespace igtlio
