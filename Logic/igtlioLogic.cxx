/*==========================================================================

  Portions (c) Copyright 2008-2009 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See Doc/copyright/copyright.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $HeadURL: http://svn.slicer.org/Slicer4/trunk/Modules/OpenIGTLinkIF/vtkSlicerOpenIGTLinkIFLogic.cxx $
  Date:      $Date: 2011-06-12 14:55:20 -0400 (Sun, 12 Jun 2011) $
  Version:   $Revision: 16985 $

==========================================================================*/

#include <algorithm>
// IGTLIO includes
#include "igtlioLogic.h"
#include "igtlioConnector.h"
#include "igtlioSession.h"

#include <vtkObjectFactory.h>

// OpenIGTLinkIF Logic includes
#include <vtkNew.h>
#include <vtkCallbackCommand.h>
#include <vtkImageData.h>
#include <vtkTransform.h>


namespace igtlio
{

//---------------------------------------------------------------------------
void onNewDeviceEventFunc(vtkObject* caller, unsigned long eid, void* clientdata, void *calldata)
{
  Logic* logic = reinterpret_cast<Logic*>(clientdata);
  logic->InvokeEvent(Logic::NewDeviceEvent, calldata);

  Device* device = reinterpret_cast<Device*>(calldata);
  device->AddObserver(Device::CommandReceivedEvent, logic->DeviceEventCallback);
  device->AddObserver(Device::CommandResponseReceivedEvent, logic->DeviceEventCallback);
}

//---------------------------------------------------------------------------
void onRemovedDeviceEventFunc(vtkObject* caller, unsigned long eid, void* clientdata, void *calldata)
{
  Logic* logic = reinterpret_cast<Logic*>(clientdata);
  logic->InvokeEvent(Logic::RemovedDeviceEvent, calldata);

  Device* device = reinterpret_cast<Device*>(calldata);
  device->RemoveObserver(logic->DeviceEventCallback);
}

//---------------------------------------------------------------------------
void onDeviceEventFunc(vtkObject* caller, unsigned long eid, void* clientdata, void *calldata)
{
  Logic* logic = reinterpret_cast<Logic*>(clientdata);

  if ((eid==Device::CommandReceivedEvent) ||
      (eid==Connector::DeviceContentModifiedEvent) ||
      (eid==Device::CommandResponseReceivedEvent))
  {
    logic->InvokeEvent(eid, calldata);
  }
}


//---------------------------------------------------------------------------
vtkStandardNewMacro(Logic);


//---------------------------------------------------------------------------
Logic::Logic()
{
  NewDeviceCallback = vtkSmartPointer<vtkCallbackCommand>::New();
  NewDeviceCallback->SetCallback(onNewDeviceEventFunc);
  NewDeviceCallback->SetClientData(this);

  RemovedDeviceCallback = vtkSmartPointer<vtkCallbackCommand>::New();
  RemovedDeviceCallback->SetCallback(onRemovedDeviceEventFunc);
  RemovedDeviceCallback->SetClientData(this);

  DeviceEventCallback = vtkSmartPointer<vtkCallbackCommand>::New();
  DeviceEventCallback->SetCallback(onDeviceEventFunc);
  DeviceEventCallback->SetClientData(this);
}

//---------------------------------------------------------------------------
Logic::~Logic()
{
}

//---------------------------------------------------------------------------
void Logic::PrintSelf(ostream& os, vtkIndent indent)
{
  os << indent << "vtkIGTLIOLogic:             " << this->GetClassName() << "\n";
}


//---------------------------------------------------------------------------
ConnectorPointer Logic::CreateConnector()
{
  ConnectorPointer connector = ConnectorPointer::New();
  connector->SetUID(this->CreateUniqueConnectorID());
  std::stringstream ss;
  ss << "IGTLConnector_" << connector->GetUID();
  connector->SetName(ss.str());
  Connectors.push_back(connector);

  connector->AddObserver(Connector::NewDeviceEvent, NewDeviceCallback);
  connector->AddObserver(Connector::DeviceContentModifiedEvent, DeviceEventCallback);
  connector->AddObserver(Connector::RemovedDeviceEvent, RemovedDeviceCallback);

  this->InvokeEvent(ConnectionAddedEvent, connector.GetPointer());
  return connector;
}

//---------------------------------------------------------------------------
int Logic::CreateUniqueConnectorID() const
{
  int retval=0;
  for (unsigned int i=0; i<Connectors.size(); ++i)
    {
      retval = std::max<int>(retval, Connectors[i]->GetUID()+1);
    }
  return retval;
}

//---------------------------------------------------------------------------
int Logic::RemoveConnector(unsigned int index)
{
  std::vector<ConnectorPointer>::iterator toRemove = Connectors.begin()+index;

  return this->RemoveConnector(toRemove);
}

//---------------------------------------------------------------------------
int Logic::RemoveConnector(ConnectorPointer connector)
{

  std::vector<ConnectorPointer>::iterator toRemove = Connectors.begin();
  for(; toRemove != Connectors.end(); ++toRemove)
  {
    if(connector == (*toRemove))
      break;
  }
  return this->RemoveConnector(toRemove);
}

//---------------------------------------------------------------------------
int Logic::RemoveConnector(std::vector<ConnectorPointer>::iterator toRemove)
{
  if(toRemove == Connectors.end())
    return 0;

  toRemove->GetPointer()->RemoveObserver(NewDeviceCallback);
  toRemove->GetPointer()->RemoveObserver(RemovedDeviceCallback);

  this->InvokeEvent(ConnectionAboutToBeRemovedEvent, toRemove->GetPointer());
  Connectors.erase(toRemove);
  return 1;
}


//---------------------------------------------------------------------------
int Logic::GetNumberOfConnectors() const
{
  return Connectors.size();
}

//---------------------------------------------------------------------------
ConnectorPointer Logic::GetConnector(unsigned int index)
{
  if (index<Connectors.size())
    return Connectors[index];

  vtkErrorMacro("index " << index << " out of bounds.");
  return NULL;
}

SessionPointer Logic::StartServer(int serverPort, igtlio::SYNCHRONIZATION_TYPE sync, double timeout_s)
{
  SessionPointer session = SessionPointer::New();
  session->SetConnector(this->CreateConnector());
  session->StartServer(serverPort, sync, timeout_s);
  return session;
}

SessionPointer Logic::ConnectToServer(std::string serverHost, int serverPort, igtlio::SYNCHRONIZATION_TYPE sync, double timeout_s)
{
  SessionPointer session = SessionPointer::New();
  session->SetConnector(this->CreateConnector());
  session->ConnectToServer(serverHost, serverPort, sync, timeout_s);
  return session;
}

//---------------------------------------------------------------------------
void Logic::PeriodicProcess()
{
  for (unsigned i=0; i<Connectors.size(); ++i)
    {
      Connectors[i]->PeriodicProcess();
    }
}

//---------------------------------------------------------------------------
unsigned int Logic::GetNumberOfDevices() const
{
  std::vector<DevicePointer> all = this->CreateDeviceList();
  return all.size();
}

//---------------------------------------------------------------------------
void Logic::RemoveDevice(unsigned int index)
{
  DevicePointer device = this->GetDevice(index);

  for (unsigned i=0; i<Connectors.size(); ++i)
    {
      for (unsigned j=0; j<Connectors[i]->GetNumberOfDevices(); ++j)
        {
          DevicePointer local = Connectors[i]->GetDevice(j);
          if (device==local)
            Connectors[i]->RemoveDevice(j);
        }
    }
}

//---------------------------------------------------------------------------
DevicePointer Logic::GetDevice(unsigned int index)
{
  // TODO: optimize by caching the vector if necessary
  std::vector<DevicePointer> all = this->CreateDeviceList();

  if (index<all.size())
    return all[index];

  vtkErrorMacro("index " << index << " out of bounds.");
  return NULL;
}

//---------------------------------------------------------------------------
int Logic::ConnectorIndexFromDevice( DevicePointer d )
{
    for( std::vector<ConnectorPointer>::size_type i = 0; i < Connectors.size(); ++i )
        if( Connectors[i]->HasDevice(d) )
            return i;
    return -1;
}

//---------------------------------------------------------------------------
std::vector<DevicePointer> Logic::CreateDeviceList() const
{
  std::set<DevicePointer> all;

  for (unsigned i=0; i<Connectors.size(); ++i)
    {
      for (unsigned j=0; j<Connectors[i]->GetNumberOfDevices(); ++j)
        {
          all.insert(Connectors[i]->GetDevice(j));
        }
    }

  return std::vector<DevicePointer>(all.begin(), all.end());
}

} // namespace igtlio

