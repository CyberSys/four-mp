#include "ReadyEvent.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "RakAssert.h"

#ifdef _MSC_VER
#pragma warning( push )
#endif

int ReadyEvent::RemoteSystemCompBySystemAddress( const SystemAddress &key, const RemoteSystem &data )
{
	if (key < data.systemAddress)
		return -1;
	else if (key==data.systemAddress)
		return 0;
	else
		return 1;
}

int ReadyEvent::ReadyEventNodeComp( const int &key, ReadyEvent::ReadyEventNode * const &data )
{
	if (key < data->eventId)
		return -1;
	else if (key==data->eventId)
		return 0;
	else
		return 1;
}


ReadyEvent::ReadyEvent()
{
	channel=0;
}

ReadyEvent::~ReadyEvent()
{
	Clear();
}


bool ReadyEvent::SetEvent(int eventId, bool isReady)
{
	bool objectExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists==false)
	{
		// Totally new event
		CreateNewEvent(eventId, isReady);
	}
	else
	{
		return SetEventByIndex(eventIndex, isReady);
	}
	return true;
}
bool ReadyEvent::ForceCompletion(int eventId)
{
	bool objectExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists==false)
	{
		// Totally new event
		CreateNewEvent(eventId, true);
		eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	}
	
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	ren->eventStatus=ID_READY_EVENT_FORCE_ALL_SET;
	UpdateReadyStatus(eventIndex);

	return true;
}
bool ReadyEvent::DeleteEvent(int eventId)
{
	bool objectExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		RakNet::OP_DELETE(readyEventNodeList[eventIndex], __FILE__, __LINE__);
		readyEventNodeList.RemoveAtIndex(eventIndex);
		return true;
	}
	return false;
}
bool ReadyEvent::IsEventSet(int eventId)
{
	bool objectExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		return readyEventNodeList[eventIndex]->eventStatus==ID_READY_EVENT_SET || readyEventNodeList[eventIndex]->eventStatus==ID_READY_EVENT_ALL_SET;
	}
	return false;
}
bool ReadyEvent::IsEventCompletionProcessing(int eventId) const
{
	bool objectExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		bool anyAllReady=false;
		bool allAllReady=true;
		ReadyEventNode *ren = readyEventNodeList[eventIndex];
		if (ren->eventStatus==ID_READY_EVENT_FORCE_ALL_SET)
			return false;
		for (unsigned i=0; i < ren->systemList.Size(); i++)
		{
			if (ren->systemList[i].lastReceivedStatus==ID_READY_EVENT_ALL_SET)
				anyAllReady=true;
			else
				allAllReady=false;
		}
		return anyAllReady==true && allAllReady==false;
	}
	return false;
}
bool ReadyEvent::IsEventCompleted(int eventId) const
{
	bool objectExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		return IsEventCompletedByIndex(eventIndex);
	}
	return false;
}

bool ReadyEvent::HasEvent(int eventId)
{
	return readyEventNodeList.HasData(eventId);
}

unsigned ReadyEvent::GetEventListSize(void) const
{
	return readyEventNodeList.Size();
}

int ReadyEvent::GetEventAtIndex(unsigned index) const
{
	return readyEventNodeList[index]->eventId;
}

bool ReadyEvent::AddToWaitList(int eventId, SystemAddress address)
{
	bool eventExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &eventExists);
	if (eventExists==false)
		eventIndex=CreateNewEvent(eventId, false);

	// Don't do this, otherwise if we are trying to start a 3 player game, it will not allow the 3rd player to hit ready if the first two players have already done so
	//if (IsLocked(eventIndex))
	//	return false; // Not in the list, but event is already completed, or is starting to complete, and adding more waiters would fail this.

	unsigned i;
	unsigned numAdded=0;
	if (address==UNASSIGNED_SYSTEM_ADDRESS)
	{
		for (i=0; i < rakPeerInterface->GetMaximumNumberOfPeers(); i++)
		{
			SystemAddress internalAddress = rakPeerInterface->GetSystemAddressFromIndex(i);
			if (internalAddress!=UNASSIGNED_SYSTEM_ADDRESS)
			{
				numAdded+=AddToWaitListInternal(eventIndex, internalAddress);
			}
		}
	}
	else
	{
		numAdded=AddToWaitListInternal(eventIndex, address);
	}

	if (numAdded>0)
		UpdateReadyStatus(eventIndex);
	return numAdded>0;
}
bool ReadyEvent::RemoveFromWaitList(int eventId, SystemAddress address)
{
	bool eventExists;
	unsigned eventIndex = readyEventNodeList.GetIndexFromKey(eventId, &eventExists);
	if (eventExists)
	{
		if (address==UNASSIGNED_SYSTEM_ADDRESS)
		{
			// Remove all waiters
			readyEventNodeList[eventIndex]->systemList.Clear(false, __FILE__, __LINE__);
			UpdateReadyStatus(eventIndex);
		}
		else
		{
			bool systemExists;
			unsigned systemIndex = readyEventNodeList[eventIndex]->systemList.GetIndexFromKey(address, &systemExists);
			if (systemExists)
			{
				bool isCompleted = IsEventCompletedByIndex(eventIndex);
				readyEventNodeList[eventIndex]->systemList.RemoveAtIndex(systemIndex);

				if (isCompleted==false && IsEventCompletedByIndex(eventIndex))
					PushCompletionPacket(readyEventNodeList[eventIndex]->eventId);

				UpdateReadyStatus(eventIndex);

				return true;
			}
		}
	}

	return false;
}
bool ReadyEvent::IsInWaitList(int eventId, SystemAddress address)
{
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		return readyEventNodeList[readyIndex]->systemList.HasData(address);
	}
	return false;
}

unsigned ReadyEvent::GetRemoteWaitListSize(int eventId) const
{
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		return readyEventNodeList[readyIndex]->systemList.Size();
	}
	return 0;
}

SystemAddress ReadyEvent::GetFromWaitListAtIndex(int eventId, unsigned index) const
{
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		return readyEventNodeList[readyIndex]->systemList[index].systemAddress;
	}
	return UNASSIGNED_SYSTEM_ADDRESS;
}
ReadyEventSystemStatus ReadyEvent::GetReadyStatus(int eventId, SystemAddress address)
{
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		ReadyEventNode *ren = readyEventNodeList[readyIndex];
		unsigned systemIndex = ren->systemList.GetIndexFromKey(address, &objectExists);
		if (objectExists==false)
			return RES_NOT_WAITING;		
		if (ren->systemList[systemIndex].lastReceivedStatus==ID_READY_EVENT_SET)
			return RES_READY;
		if (ren->systemList[systemIndex].lastReceivedStatus==ID_READY_EVENT_UNSET)
			return RES_WAITING;
		if (ren->systemList[systemIndex].lastReceivedStatus==ID_READY_EVENT_ALL_SET)
			return RES_ALL_READY;
	}

	return RES_UNKNOWN_EVENT;
}
void ReadyEvent::SetSendChannel(unsigned char newChannel)
{
	channel=newChannel;
}
PluginReceiveResult ReadyEvent::OnReceive(Packet *packet)
{
	unsigned char packetIdentifier;
	packetIdentifier = ( unsigned char ) packet->data[ 0 ];

//	bool doPrint = packet->systemAddress.port==60002 || rakPeerInterface->GetInternalID(UNASSIGNED_SYSTEM_ADDRESS).port==60002;

	switch (packetIdentifier)
	{
	case ID_READY_EVENT_UNSET:
	case ID_READY_EVENT_SET:
	case ID_READY_EVENT_ALL_SET:
//		if (doPrint) {if (packet->systemAddress.port==60002)	RAKNET_DEBUG_PRINTF("FROM 60002: "); else if (rakPeerInterface->GetInternalID(UNASSIGNED_SYSTEM_ADDRESS).port==60002)	RAKNET_DEBUG_PRINTF("TO 60002: "); RAKNET_DEBUG_PRINTF("ID_READY_EVENT_SET\n");}
		OnReadyEventPacketUpdate(packet);
		return RR_CONTINUE_PROCESSING;
	case ID_READY_EVENT_FORCE_ALL_SET:
		OnReadyEventForceAllSet(packet);
		return RR_CONTINUE_PROCESSING;
	case ID_READY_EVENT_QUERY:
//		if (doPrint) {if (packet->systemAddress.port==60002)	RAKNET_DEBUG_PRINTF("FROM 60002: "); else if (rakPeerInterface->GetInternalID(UNASSIGNED_SYSTEM_ADDRESS).port==60002)	RAKNET_DEBUG_PRINTF("TO 60002: "); RAKNET_DEBUG_PRINTF("ID_READY_EVENT_QUERY\n");}
		OnReadyEventQuery(packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	}

	return RR_CONTINUE_PROCESSING;
}
bool ReadyEvent::AddToWaitListInternal(unsigned eventIndex, SystemAddress address)
{
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	bool objectExists;
	unsigned systemIndex = ren->systemList.GetIndexFromKey(address, &objectExists);
	if (objectExists==false)
	{
		RemoteSystem rs;
		rs.lastReceivedStatus=ID_READY_EVENT_UNSET;
		rs.lastSentStatus=ID_READY_EVENT_UNSET;
		rs.systemAddress=address;
		ren->systemList.InsertAtIndex(rs,systemIndex, __FILE__,__LINE__);

		SendReadyStateQuery(ren->eventId, address);
		return true;
	}
	return false;
}
void ReadyEvent::OnReadyEventForceAllSet(Packet *packet)
{
	RakNet::BitStream incomingBitStream(packet->data, packet->length, false);
	incomingBitStream.IgnoreBits(8);
	int eventId;
	incomingBitStream.Read(eventId);
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		ReadyEventNode *ren = readyEventNodeList[readyIndex];
		if (ren->eventStatus!=ID_READY_EVENT_FORCE_ALL_SET)
		{
			ren->eventStatus=ID_READY_EVENT_FORCE_ALL_SET;
			PushCompletionPacket(ren->eventId);
		}
	}
}
void ReadyEvent::OnReadyEventPacketUpdate(Packet *packet)
{
	RakNet::BitStream incomingBitStream(packet->data, packet->length, false);
	incomingBitStream.IgnoreBits(8);
	int eventId;
	incomingBitStream.Read(eventId);
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		ReadyEventNode *ren = readyEventNodeList[readyIndex];
		bool systemExists;
		unsigned systemIndex = ren->systemList.GetIndexFromKey(packet->systemAddress, &systemExists);
		if (systemExists)
		{
			// Just return if no change
			if (ren->systemList[systemIndex].lastReceivedStatus==packet->data[0])
				return;

			bool wasCompleted = IsEventCompletedByIndex(readyIndex);
			ren->systemList[systemIndex].lastReceivedStatus=packet->data[0];
			// If forced all set, doesn't matter what the new packet is
			if (ren->eventStatus==ID_READY_EVENT_FORCE_ALL_SET)
				return;
			UpdateReadyStatus(readyIndex);
			if (wasCompleted==false && IsEventCompletedByIndex(readyIndex))
				PushCompletionPacket(readyIndex);
		}
	}
}
void ReadyEvent::OnReadyEventQuery(Packet *packet)
{
	RakNet::BitStream incomingBitStream(packet->data, packet->length, false);
	incomingBitStream.IgnoreBits(8);
	int eventId;
	incomingBitStream.Read(eventId);
	bool objectExists;
	unsigned readyIndex = readyEventNodeList.GetIndexFromKey(eventId, &objectExists);
	if (objectExists)
	{
		unsigned systemIndex = readyEventNodeList[readyIndex]->systemList.GetIndexFromKey(packet->systemAddress,&objectExists);
		// Force the non-default send, because our initial send may have arrived at a system that didn't yet create the ready event
		if (objectExists)
			SendReadyUpdate(readyIndex, systemIndex, true);
	}
}
void ReadyEvent::OnClosedConnection(SystemAddress systemAddress, RakNetGUID rakNetGUID, PI2_LostConnectionReason lostConnectionReason )
{
	(void) systemAddress;
	(void) rakNetGUID;
	(void) lostConnectionReason;

	RemoveFromAllLists(systemAddress);
}
void ReadyEvent::OnRakPeerShutdown(void)
{
	Clear();
}

bool ReadyEvent::SetEventByIndex(int eventIndex, bool isReady)
{
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	if ((ren->eventStatus==ID_READY_EVENT_ALL_SET || ren->eventStatus==ID_READY_EVENT_SET) && isReady==true)
		return true; // Success - no change
	if (ren->eventStatus==ID_READY_EVENT_UNSET && isReady==false)
		return true; // Success - no change
	if (ren->eventStatus==ID_READY_EVENT_FORCE_ALL_SET)
		return true; // Can't change

	if (isReady)
		ren->eventStatus=ID_READY_EVENT_SET;
	else
		ren->eventStatus=ID_READY_EVENT_UNSET;

	UpdateReadyStatus(eventIndex);

	// Check if now completed, and if so, tell the user about it
	if (IsEventCompletedByIndex(eventIndex))
	{
		PushCompletionPacket(ren->eventId);
	}

	return true;
}

bool ReadyEvent::IsEventCompletedByIndex(unsigned eventIndex) const
{
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	unsigned i;
	if (ren->eventStatus==ID_READY_EVENT_FORCE_ALL_SET)
		return true;
	if (ren->eventStatus!=ID_READY_EVENT_ALL_SET)
		return false;
	for (i=0; i < ren->systemList.Size(); i++)
		if (ren->systemList[i].lastReceivedStatus!=ID_READY_EVENT_ALL_SET)
			return false;
	return true;
}

void ReadyEvent::Clear(void)
{
	unsigned i;
	for (i=0; i < readyEventNodeList.Size(); i++)
	{
		RakNet::OP_DELETE(readyEventNodeList[i], __FILE__, __LINE__);
	}
	readyEventNodeList.Clear(false, __FILE__, __LINE__);
}

unsigned ReadyEvent::CreateNewEvent(int eventId, bool isReady)
{
	ReadyEventNode *ren = RakNet::OP_NEW<ReadyEventNode>( __FILE__, __LINE__ );
	ren->eventId=eventId;
	if (isReady==false)
		ren->eventStatus=ID_READY_EVENT_UNSET;
	else
		ren->eventStatus=ID_READY_EVENT_SET;
	return readyEventNodeList.Insert(eventId, ren, true, __FILE__,__LINE__);
}
void ReadyEvent::UpdateReadyStatus(unsigned eventIndex)
{
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	bool anyUnset;
	unsigned i;
	if (ren->eventStatus==ID_READY_EVENT_SET)
	{
		// If you are set, and no other systems are ID_READY_EVENT_UNSET, then change your status to ID_READY_EVENT_ALL_SET
		anyUnset=false;
		for (i=0; i < ren->systemList.Size(); i++)
		{
			if (ren->systemList[i].lastReceivedStatus==ID_READY_EVENT_UNSET)
			{
				anyUnset=true;
				break;
			}
		}
		if (anyUnset==false)
		{
			ren->eventStatus=ID_READY_EVENT_ALL_SET;
		}
	}
	else if (ren->eventStatus==ID_READY_EVENT_ALL_SET)
	{
		// If you are all set, and any systems are ID_READY_EVENT_UNSET, then change your status to ID_READY_EVENT_SET
		anyUnset=false;
		for (i=0; i < ren->systemList.Size(); i++)
		{
			if (ren->systemList[i].lastReceivedStatus==ID_READY_EVENT_UNSET)
			{
				anyUnset=true;
				break;
			}
		}
		if (anyUnset==true)
		{
			ren->eventStatus=ID_READY_EVENT_SET;
		}
	}
	BroadcastReadyUpdate(eventIndex, false);
}
void ReadyEvent::SendReadyUpdate(unsigned eventIndex, unsigned systemIndex, bool forceIfNotDefault)
{
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	RakNet::BitStream bs;
	// I do this rather than write true or false, so users that do not use BitStreams can still read the data
	if ((ren->eventStatus!=ren->systemList[systemIndex].lastSentStatus) ||
		(forceIfNotDefault && ren->eventStatus!=ID_READY_EVENT_UNSET))
	{
		bs.Write(ren->eventStatus);
		bs.Write(ren->eventId);
		SendUnified(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, channel, ren->systemList[systemIndex].systemAddress, false);

		ren->systemList[systemIndex].lastSentStatus=ren->eventStatus;
	}
	
}
void ReadyEvent::BroadcastReadyUpdate(unsigned eventIndex, bool forceIfNotDefault)
{
	ReadyEventNode *ren = readyEventNodeList[eventIndex];
	unsigned systemIndex;
	for (systemIndex=0; systemIndex < ren->systemList.Size(); systemIndex++)
	{
		SendReadyUpdate(eventIndex, systemIndex, forceIfNotDefault);
	}
}
void ReadyEvent::SendReadyStateQuery(unsigned eventId, SystemAddress address)
{
	RakNet::BitStream bs;
	bs.Write((MessageID)ID_READY_EVENT_QUERY);
	bs.Write(eventId);
	SendUnified(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, channel, address, false);
}
void ReadyEvent::RemoveFromAllLists(SystemAddress address)
{
	unsigned eventIndex;
	for (eventIndex=0; eventIndex < readyEventNodeList.Size(); eventIndex++)
	{
		bool isCompleted = IsEventCompletedByIndex(eventIndex);
		bool systemExists;
		unsigned systemIndex;
		
		systemIndex = readyEventNodeList[eventIndex]->systemList.GetIndexFromKey(address, &systemExists);
		if (systemExists)
			readyEventNodeList[eventIndex]->systemList.RemoveAtIndex(systemIndex);
	
		UpdateReadyStatus(eventIndex);

		if (isCompleted==false && IsEventCompletedByIndex(eventIndex))
			PushCompletionPacket(readyEventNodeList[eventIndex]->eventId);
	}
}
void ReadyEvent::PushCompletionPacket(unsigned eventId)
{
	(void) eventId;
	// Not necessary
	/*
	// Pass a packet to the user that we are now completed, as setting ourselves to signaled was the last thing being waited on
	Packet *p = rakPeerInterface->AllocatePacket(sizeof(MessageID)+sizeof(int));
	RakNet::BitStream bs(p->data, sizeof(MessageID)+sizeof(int), false);
	bs.SetWriteOffset(0);
	bs.Write((MessageID)ID_READY_EVENT_ALL_SET);
	bs.Write(eventId);
	rakPeerInterface->PushBackPacket(p, false);
	*/
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif
