#include "PacketizedTCP.h"
#include "NativeTypes.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "RakAlloca.h"

typedef uint32_t PTCPHeader;

PacketizedTCP::PacketizedTCP()
{

}
PacketizedTCP::~PacketizedTCP()
{
	ClearAllConnections();
}

bool PacketizedTCP::Start(unsigned short port, unsigned short maxIncomingConnections, int threadPriority)
{
	bool success = TCPInterface::Start(port, maxIncomingConnections,0,threadPriority);
	if (success)
	{
		unsigned int i;
		for (i=0; i < messageHandlerList.Size(); i++)
			messageHandlerList[i]->OnRakPeerStartup();
	}
	return success;
}

void PacketizedTCP::Stop(void)
{
	unsigned int i;
	for (i=0; i < messageHandlerList.Size(); i++)
		messageHandlerList[i]->OnRakPeerShutdown();
	for (i=0; i < waitingPackets.Size(); i++)
		DeallocatePacket(waitingPackets[i]);
	TCPInterface::Stop();
	ClearAllConnections();
}

void PacketizedTCP::Send( const char *data, unsigned length, SystemAddress systemAddress, bool broadcast )
{
	PTCPHeader dataLength;
	dataLength=length;
#ifndef __BITSTREAM_NATIVE_END
	if (RakNet::BitStream::DoEndianSwap())
		RakNet::BitStream::ReverseBytes((unsigned char*) &length,(unsigned char*) &dataLength,sizeof(dataLength));
#else
		dataLength=length;
#endif

	unsigned int lengthsArray[2];
	const char *dataArray[2];
	dataArray[0]=(char*) &dataLength;
	dataArray[1]=data;
	lengthsArray[0]=sizeof(dataLength);
	lengthsArray[1]=length;
	TCPInterface::SendList(dataArray,lengthsArray,2,systemAddress,broadcast);
}
bool PacketizedTCP::SendList( const char **data, const int *lengths, const int numParameters, SystemAddress systemAddress, bool broadcast )
{
	if (isStarted==false)
		return false;
	if (data==0)
		return false;
	if (systemAddress==UNASSIGNED_SYSTEM_ADDRESS && broadcast==false)
		return false;
	PTCPHeader totalLengthOfUserData=0;
	int i;
	for (i=0; i < numParameters; i++)
	{
		if (lengths[i]>0)
			totalLengthOfUserData+=lengths[i];
	}
	if (totalLengthOfUserData==0)
		return false;

	PTCPHeader dataLength;
#ifndef __BITSTREAM_NATIVE_END
	if (RakNet::BitStream::DoEndianSwap())
		RakNet::BitStream::ReverseBytes((unsigned char*) &totalLengthOfUserData,(unsigned char*) &dataLength,sizeof(dataLength));
#else
	dataLength=totalLengthOfUserData;
#endif
	

	unsigned int lengthsArray[512];
	const char *dataArray[512];
	dataArray[0]=(char*) &dataLength;
	lengthsArray[0]=sizeof(dataLength);
	for (int i=0; i < 512 && i < numParameters; i++)
	{
		dataArray[i+1]=data[i];
		lengthsArray[i+1]=lengths[i];
	}	
	return TCPInterface::SendList(dataArray,lengthsArray,numParameters+1,systemAddress,broadcast);
}
void PacketizedTCP::PushNotificationsToQueues(void)
{
	SystemAddress sa;
	sa = TCPInterface::HasNewIncomingConnection();
	if (sa!=UNASSIGNED_SYSTEM_ADDRESS)
	{
		_newIncomingConnections.Push(sa, __FILE__, __LINE__ );
		AddToConnectionList(sa);
		unsigned int i;
		for (i=0; i < messageHandlerList.Size(); i++)
			messageHandlerList[i]->OnNewConnection(sa, UNASSIGNED_RAKNET_GUID, true);
	}

	sa = TCPInterface::HasFailedConnectionAttempt();
	if (sa!=UNASSIGNED_SYSTEM_ADDRESS)
	{
		_failedConnectionAttempts.Push(sa, __FILE__, __LINE__ );
		unsigned int i;
		for (i=0; i < messageHandlerList.Size(); i++)
			messageHandlerList[i]->OnFailedConnectionAttempt(sa, FCAR_CONNECTION_ATTEMPT_FAILED);
	}

	sa = TCPInterface::HasLostConnection();
	if (sa!=UNASSIGNED_SYSTEM_ADDRESS)
	{
		_lostConnections.Push(sa, __FILE__, __LINE__ );
		RemoveFromConnectionList(sa);
		unsigned int i;
		for (i=0; i < messageHandlerList.Size(); i++)
			messageHandlerList[i]->OnClosedConnection(sa, UNASSIGNED_RAKNET_GUID, LCR_DISCONNECTION_NOTIFICATION);
	}

	sa = TCPInterface::HasCompletedConnectionAttempt();
	if (sa!=UNASSIGNED_SYSTEM_ADDRESS)
	{
		_completedConnectionAttempts.Push(sa, __FILE__, __LINE__ );
		AddToConnectionList(sa);
		unsigned int i;
		for (i=0; i < messageHandlerList.Size(); i++)
			messageHandlerList[i]->OnNewConnection(sa, UNASSIGNED_RAKNET_GUID, true);
	}
}
Packet* PacketizedTCP::Receive( void )
{
	PushNotificationsToQueues();

	unsigned int i;
	for (i=0; i < messageHandlerList.Size(); i++)
		messageHandlerList[i]->Update();

	Packet *outgoingPacket=ReturnOutgoingPacket();
	if (outgoingPacket)
		return outgoingPacket;

	Packet *incomingPacket;
	incomingPacket = TCPInterface::Receive();
	unsigned int index;

	while (incomingPacket)
	{
		if (connections.Has(incomingPacket->systemAddress))
			index = connections.GetIndexAtKey(incomingPacket->systemAddress);
		else
			index=(unsigned int) -1;
		if ((unsigned int)index==(unsigned int)-1)
		{
			DeallocatePacket(incomingPacket);
			incomingPacket = TCPInterface::Receive();
			continue;
		}


		if (incomingPacket->deleteData==true)
		{
			// Came from network
			SystemAddress systemAddressFromPacket;
			if (index < connections.Size())
			{
				DataStructures::ByteQueue *bq = connections[index];
				// Buffer data
				bq->WriteBytes((const char*) incomingPacket->data,incomingPacket->length, __FILE__,__LINE__);
				systemAddressFromPacket=incomingPacket->systemAddress;
				PTCPHeader dataLength;

				// Peek the header to see if a full message is waiting
				bq->ReadBytes((char*) &dataLength,sizeof(PTCPHeader),true);
				if (RakNet::BitStream::DoEndianSwap())
					RakNet::BitStream::ReverseBytesInPlace((unsigned char*) &dataLength,sizeof(dataLength));
				// Header indicates packet length. If enough data is available, read out and return one packet
				if (bq->GetBytesWritten()>=dataLength+sizeof(PTCPHeader))
				{
					do 
					{
						bq->IncrementReadOffset(sizeof(PTCPHeader));
						outgoingPacket = RakNet::OP_NEW<Packet>(__FILE__, __LINE__);
						outgoingPacket->length=dataLength;
						outgoingPacket->bitSize=BYTES_TO_BITS(dataLength);
						outgoingPacket->guid=UNASSIGNED_RAKNET_GUID;
						outgoingPacket->systemAddress=systemAddressFromPacket;
						outgoingPacket->deleteData=false; // Did not come from the network
						outgoingPacket->data=(unsigned char*) rakMalloc_Ex(dataLength, __FILE__, __LINE__);
						if (outgoingPacket->data==0)
						{
							notifyOutOfMemory(__FILE__, __LINE__);
							RakNet::OP_DELETE(outgoingPacket,__FILE__,__LINE__);
							return 0;
						}
						bq->ReadBytes((char*) outgoingPacket->data,dataLength,false);

						waitingPackets.Push(outgoingPacket, __FILE__, __LINE__ );

						// Peek the header to see if a full message is waiting
						if (bq->ReadBytes((char*) &dataLength,sizeof(PTCPHeader),true))
						{
							if (RakNet::BitStream::DoEndianSwap())
								RakNet::BitStream::ReverseBytesInPlace((unsigned char*) &dataLength,sizeof(dataLength));
						}
						else
							break;
					} while (bq->GetBytesWritten()>=dataLength+sizeof(PTCPHeader));
				}
				else
				{

					unsigned int oldWritten = bq->GetBytesWritten()-incomingPacket->length;
					unsigned int newWritten = bq->GetBytesWritten();

					// Return ID_DOWNLOAD_PROGRESS
					if (newWritten/65536!=oldWritten/65536)
					{
						outgoingPacket = RakNet::OP_NEW<Packet>(__FILE__, __LINE__);
						outgoingPacket->length=sizeof(MessageID) +
							sizeof(unsigned int)*2 +
							sizeof(unsigned int) +
							65536;
						outgoingPacket->bitSize=BYTES_TO_BITS(incomingPacket->length);
						outgoingPacket->guid=UNASSIGNED_RAKNET_GUID;
						outgoingPacket->systemAddress=incomingPacket->systemAddress;
						outgoingPacket->deleteData=false;
						outgoingPacket->data=(unsigned char*) rakMalloc_Ex(outgoingPacket->length, __FILE__, __LINE__);
						if (outgoingPacket->data==0)
						{
							notifyOutOfMemory(__FILE__, __LINE__);
							RakNet::OP_DELETE(outgoingPacket,__FILE__,__LINE__);
							return 0;
						}

						outgoingPacket->data[0]=(MessageID)ID_DOWNLOAD_PROGRESS;
						unsigned int totalParts=dataLength/65536;
						unsigned int partIndex=newWritten/65536;
						unsigned int oneChunkSize=65536;
						memcpy(outgoingPacket->data+sizeof(MessageID), &partIndex, sizeof(unsigned int));
						memcpy(outgoingPacket->data+sizeof(MessageID)+sizeof(unsigned int)*1, &totalParts, sizeof(unsigned int));
						memcpy(outgoingPacket->data+sizeof(MessageID)+sizeof(unsigned int)*2, &oneChunkSize, sizeof(unsigned int));
						bq->IncrementReadOffset(sizeof(PTCPHeader));
						bq->ReadBytes((char*) outgoingPacket->data+sizeof(MessageID)+sizeof(unsigned int)*3,oneChunkSize,true);
						bq->DecrementReadOffset(sizeof(PTCPHeader));

						waitingPackets.Push(outgoingPacket, __FILE__, __LINE__ );
					}
				}

			}

			DeallocatePacket(incomingPacket);
			incomingPacket=0;
		}
		else
			waitingPackets.Push(incomingPacket, __FILE__, __LINE__ );

		incomingPacket = TCPInterface::Receive();
	}

	return ReturnOutgoingPacket();
}
Packet *PacketizedTCP::ReturnOutgoingPacket(void)
{
	Packet *outgoingPacket=0;
	unsigned int i;
	while (outgoingPacket==0 && waitingPackets.IsEmpty()==false)
	{
		outgoingPacket=waitingPackets.Pop();
		PluginReceiveResult pluginResult;
		for (i=0; i < messageHandlerList.Size(); i++)
		{
			pluginResult=messageHandlerList[i]->OnReceive(outgoingPacket);
			if (pluginResult==RR_STOP_PROCESSING_AND_DEALLOCATE)
			{
				DeallocatePacket( outgoingPacket );
				outgoingPacket=0; // Will do the loop again and get another packet
				break; // break out of the enclosing for
			}
			else if (pluginResult==RR_STOP_PROCESSING)
			{
				outgoingPacket=0;
				break;
			}
		}
	}

	return outgoingPacket;
}

void PacketizedTCP::AttachPlugin( PluginInterface2 *plugin )
{
	if (messageHandlerList.GetIndexOf(plugin)==MAX_UNSIGNED_LONG)
	{
		messageHandlerList.Insert(plugin, __FILE__, __LINE__);
		plugin->SetPacketizedTCP(this);
		plugin->OnAttach();
	}
}
void PacketizedTCP::DetachPlugin( PluginInterface2 *plugin )
{
	if (plugin==0)
		return;

	unsigned int index;
	index = messageHandlerList.GetIndexOf(plugin);
	if (index!=MAX_UNSIGNED_LONG)
	{
		messageHandlerList[index]->OnDetach();
		// Unordered list so delete from end for speed
		messageHandlerList[index]=messageHandlerList[messageHandlerList.Size()-1];
		messageHandlerList.RemoveFromEnd();
		plugin->SetPacketizedTCP(0);
	}
}
void PacketizedTCP::CloseConnection( SystemAddress systemAddress )
{
	RemoveFromConnectionList(systemAddress);
	unsigned int i;
	for (i=0; i < messageHandlerList.Size(); i++)
		messageHandlerList[i]->OnClosedConnection(systemAddress, UNASSIGNED_RAKNET_GUID, LCR_CLOSED_BY_USER);
	TCPInterface::CloseConnection(systemAddress);
}
void PacketizedTCP::RemoveFromConnectionList(SystemAddress sa)
{
	if (sa==UNASSIGNED_SYSTEM_ADDRESS)
		return;
	if (connections.Has(sa))
	{
		unsigned int index = connections.GetIndexAtKey(sa);
		if (index!=(unsigned int)-1)
		{
			RakNet::OP_DELETE(connections[index],__FILE__,__LINE__);
			connections.RemoveAtIndex(index);
		}
	}
}
void PacketizedTCP::AddToConnectionList(SystemAddress sa)
{
	if (sa==UNASSIGNED_SYSTEM_ADDRESS)
		return;
	connections.SetNew(sa, RakNet::OP_NEW<DataStructures::ByteQueue>(__FILE__, __LINE__));
}
void PacketizedTCP::ClearAllConnections(void)
{
	unsigned int i;
	for (i=0; i < connections.Size(); i++)
		RakNet::OP_DELETE(connections[i],__FILE__,__LINE__);
	connections.Clear();
}
SystemAddress PacketizedTCP::HasCompletedConnectionAttempt(void)
{
	PushNotificationsToQueues();

	if (_completedConnectionAttempts.IsEmpty()==false)
		return _completedConnectionAttempts.Pop();
	return UNASSIGNED_SYSTEM_ADDRESS;
}
SystemAddress PacketizedTCP::HasFailedConnectionAttempt(void)
{
	PushNotificationsToQueues();

	if (_failedConnectionAttempts.IsEmpty()==false)
		return _failedConnectionAttempts.Pop();
	return UNASSIGNED_SYSTEM_ADDRESS;
}
SystemAddress PacketizedTCP::HasNewIncomingConnection(void)
{
	PushNotificationsToQueues();

	if (_newIncomingConnections.IsEmpty()==false)
		return _newIncomingConnections.Pop();
	return UNASSIGNED_SYSTEM_ADDRESS;
}
SystemAddress PacketizedTCP::HasLostConnection(void)
{
	PushNotificationsToQueues();

	if (_lostConnections.IsEmpty()==false)
		return _lostConnections.Pop();
	return UNASSIGNED_SYSTEM_ADDRESS;
}
