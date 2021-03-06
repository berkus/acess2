/*
 * Acess2 IP Stack
 * - TCP Handling
 */
#define DEBUG	0
#include "ipstack.h"
#include "ipv4.h"
#include "ipv6.h"
#include "tcp.h"

#define USE_SELECT	1
#define HEXDUMP_INCOMING	0
#define HEXDUMP_OUTGOING	0
#define	CACHE_FUTURE_PACKETS_IN_BYTES	1	// Use a ring buffer to cache out of order packets

#define TCP_MIN_DYNPORT	0xC000
#define TCP_MAX_HALFOPEN	1024	// Should be enough

#define TCP_MAX_PACKET_SIZE	1024
#define TCP_WINDOW_SIZE	0x2000
#define TCP_RECIEVE_BUFFER_SIZE	0x8000
#define TCP_DACK_THRESHOLD	4096
#define TCP_DACK_TIMEOUT	500

// === PROTOTYPES ===
void	TCP_Initialise(void);
void	TCP_StartConnection(tTCPConnection *Conn);
void	TCP_SendPacket(tTCPConnection *Conn, tTCPHeader *Header, size_t DataLen, const void *Data);
void	TCP_GetPacket(tInterface *Interface, void *Address, int Length, void *Buffer);
void	TCP_INT_HandleConnectionPacket(tTCPConnection *Connection, tTCPHeader *Header, int Length);
int	TCP_INT_AppendRecieved(tTCPConnection *Connection, const void *Data, size_t Length);
void	TCP_INT_UpdateRecievedFromFuture(tTCPConnection *Connection);
void	TCP_INT_SendACK(tTCPConnection *Connection);
Uint16	TCP_GetUnusedPort();
 int	TCP_AllocatePort(Uint16 Port);
 int	TCP_DeallocatePort(Uint16 Port);
// --- Server
tVFS_Node	*TCP_Server_Init(tInterface *Interface);
 int	TCP_Server_ReadDir(tVFS_Node *Node, int Pos, char Name[FILENAME_MAX]);
tVFS_Node	*TCP_Server_FindDir(tVFS_Node *Node, const char *Name, Uint Flags);
 int	TCP_Server_IOCtl(tVFS_Node *Node, int ID, void *Data);
void	TCP_Server_Close(tVFS_Node *Node);
// --- Client
tVFS_Node	*TCP_Client_Init(tInterface *Interface);
size_t	TCP_Client_Read(tVFS_Node *Node, off_t Offset, size_t Length, void *Buffer, Uint Flags);
size_t	TCP_Client_Write(tVFS_Node *Node, off_t Offset, size_t Length, const void *Buffer, Uint Flags);
 int	TCP_Client_IOCtl(tVFS_Node *Node, int ID, void *Data);
void	TCP_Client_Close(tVFS_Node *Node);
// --- Helpers
 int	WrapBetween(Uint32 Lower, Uint32 Value, Uint32 Higher, Uint32 MaxValue);

// === TEMPLATES ===
tSocketFile	gTCP_ServerFile = {NULL, "tcps", TCP_Server_Init};
tSocketFile	gTCP_ClientFile = {NULL, "tcpc", TCP_Client_Init};
tVFS_NodeType	gTCP_ServerNodeType = {
	.TypeName = "TCP Server",
	.ReadDir = TCP_Server_ReadDir,
	.FindDir = TCP_Server_FindDir,
	.IOCtl   = TCP_Server_IOCtl,
	.Close   = TCP_Server_Close
	};
tVFS_NodeType	gTCP_ClientNodeType = {
	.TypeName = "TCP Client/Connection",
	.Read  = TCP_Client_Read,
	.Write = TCP_Client_Write,
	.IOCtl = TCP_Client_IOCtl,
	.Close = TCP_Client_Close
	};

// === GLOBALS ===
 int	giTCP_NumHalfopen = 0;
tShortSpinlock	glTCP_Listeners;
tTCPListener	*gTCP_Listeners;
tShortSpinlock	glTCP_OutbountCons;
tTCPConnection	*gTCP_OutbountCons;
Uint32	gaTCP_PortBitmap[0x800];
 int	giTCP_NextOutPort = TCP_MIN_DYNPORT;

// === CODE ===
/**
 * \brief Initialise the TCP Layer
 * 
 * Registers the client and server files and the GetPacket callback
 */
void TCP_Initialise(void)
{
	giTCP_NextOutPort += rand()%32;
	IPStack_AddFile(&gTCP_ServerFile);
	IPStack_AddFile(&gTCP_ClientFile);
	IPv4_RegisterCallback(IP4PROT_TCP, TCP_GetPacket);
	IPv6_RegisterCallback(IP4PROT_TCP, TCP_GetPacket);
}

/**
 * \brief Sends a packet from the specified connection, calculating the checksums
 * \param Conn	Connection
 * \param Length	Length of data
 * \param Data	Packet data (cast as a TCP Header)
 */
void TCP_SendPacket( tTCPConnection *Conn, tTCPHeader *Header, size_t Length, const void *Data )
{
	tIPStackBuffer	*buffer;
	Uint16	checksum[3];
	 int	packlen = sizeof(*Header) + Length;
	
	buffer = IPStack_Buffer_CreateBuffer(2 + IPV4_BUFFERS);
	if( Data && Length )
		IPStack_Buffer_AppendSubBuffer(buffer, Length, 0, Data, NULL, NULL);
	IPStack_Buffer_AppendSubBuffer(buffer, sizeof(*Header), 0, Header, NULL, NULL);

	LOG("Sending %i+%i to %s:%i", sizeof(*Header), Length,
		IPStack_PrintAddress(Conn->Interface->Type, &Conn->RemoteIP),
		Conn->RemotePort
		);

	Header->Checksum = 0;
	checksum[1] = htons( ~IPv4_Checksum(Header, sizeof(tTCPHeader)) );
	checksum[2] = htons( ~IPv4_Checksum(Data, Length) );
	
	// TODO: Fragment packet
	
	switch( Conn->Interface->Type )
	{
	case 4:
		// Get IPv4 pseudo-header checksum
		{
			Uint32	buf[3];
			buf[0] = ((tIPv4*)Conn->Interface->Address)->L;
			buf[1] = Conn->RemoteIP.v4.L;
			buf[2] = (htons(packlen)<<16) | (6<<8) | 0;
			checksum[0] = htons( ~IPv4_Checksum(buf, sizeof(buf)) );	// Partial checksum
		}
		// - Combine checksums
		Header->Checksum = htons( IPv4_Checksum(checksum, sizeof(checksum)) );
		IPv4_SendPacket(Conn->Interface, Conn->RemoteIP.v4, IP4PROT_TCP, 0, buffer);
		break;
		
	case 6:
		// Append IPv6 Pseudo Header
		{
			Uint32	buf[4+4+1+1];
			memcpy(buf, Conn->Interface->Address, 16);
			memcpy(&buf[4], &Conn->RemoteIP, 16);
			buf[8] = htonl(packlen);
			buf[9] = htonl(6);
			checksum[0] = htons( ~IPv4_Checksum(buf, sizeof(buf)) );	// Partial checksum
		}
		Header->Checksum = htons( IPv4_Checksum(checksum, sizeof(checksum)) );	// Combine the two
		IPv6_SendPacket(Conn->Interface, Conn->RemoteIP.v6, IP4PROT_TCP, Length, Data);
		break;
	}
}

/**
 * \brief Handles a packet from the IP Layer
 * \param Interface	Interface the packet arrived from
 * \param Address	Pointer to the addres structure
 * \param Length	Size of packet in bytes
 * \param Buffer	Packet data
 */
void TCP_GetPacket(tInterface *Interface, void *Address, int Length, void *Buffer)
{
	tTCPHeader	*hdr = Buffer;
	tTCPListener	*srv;
	tTCPConnection	*conn;

	Log_Log("TCP", "TCP_GetPacket: <Local>:%i from [%s]:%i, Flags = %s%s%s%s%s%s%s%s",
		ntohs(hdr->DestPort),
		IPStack_PrintAddress(Interface->Type, Address),
		ntohs(hdr->SourcePort),
		(hdr->Flags & TCP_FLAG_CWR) ? "CWR " : "",
		(hdr->Flags & TCP_FLAG_ECE) ? "ECE " : "",
		(hdr->Flags & TCP_FLAG_URG) ? "URG " : "",
		(hdr->Flags & TCP_FLAG_ACK) ? "ACK " : "",
		(hdr->Flags & TCP_FLAG_PSH) ? "PSH " : "",
		(hdr->Flags & TCP_FLAG_RST) ? "RST " : "",
		(hdr->Flags & TCP_FLAG_SYN) ? "SYN " : "",
		(hdr->Flags & TCP_FLAG_FIN) ? "FIN " : ""
		);

	if( Length > (hdr->DataOffset >> 4)*4 )
	{
		LOG("SequenceNumber = 0x%x", ntohl(hdr->SequenceNumber));
#if HEXDUMP_INCOMING
		Debug_HexDump(
			"TCP_GetPacket: Packet Data = ",
			(Uint8*)hdr + (hdr->DataOffset >> 4)*4,
			Length - (hdr->DataOffset >> 4)*4
			);
#endif
	}

	// Check Servers
	for( srv = gTCP_Listeners; srv; srv = srv->Next )
	{
		// Check if the server is active
		if(srv->Port == 0)	continue;
		// Check the interface
		if(srv->Interface && srv->Interface != Interface)	continue;
		// Check the destination port
		if(srv->Port != htons(hdr->DestPort))	continue;
		
		Log_Log("TCP", "TCP_GetPacket: Matches server %p", srv);
		// Is this in an established connection?
		for( conn = srv->Connections; conn; conn = conn->Next )
		{
			// Check that it is coming in on the same interface
			if(conn->Interface != Interface)	continue;

			// Check Source Port
			Log_Log("TCP", "TCP_GetPacket: conn->RemotePort(%i) == hdr->SourcePort(%i)",
				conn->RemotePort, ntohs(hdr->SourcePort));
			if(conn->RemotePort != ntohs(hdr->SourcePort))	continue;

			// Check Source IP
			Log_Debug("TCP", "TCP_GetPacket: conn->RemoteIP(%s)",
				IPStack_PrintAddress(conn->Interface->Type, &conn->RemoteIP));
			Log_Debug("TCP", "                == Address(%s)",
				IPStack_PrintAddress(conn->Interface->Type, Address));
			if( IPStack_CompareAddress(conn->Interface->Type, &conn->RemoteIP, Address, -1) == 0 )
				continue ;

			Log_Log("TCP", "TCP_GetPacket: Matches connection %p", conn);
			// We have a response!
			TCP_INT_HandleConnectionPacket(conn, hdr, Length);

			return;
		}

		Log_Log("TCP", "TCP_GetPacket: Opening Connection");
		// Open a new connection (well, check that it's a SYN)
		if(hdr->Flags != TCP_FLAG_SYN) {
			Log_Log("TCP", "TCP_GetPacket: Packet is not a SYN");
			return ;
		}
		
		// TODO: Check for halfopen max
		
		conn = calloc(1, sizeof(tTCPConnection));
		conn->State = TCP_ST_SYN_RCVD;
		conn->LocalPort = srv->Port;
		conn->RemotePort = ntohs(hdr->SourcePort);
		conn->Interface = Interface;
		
		switch(Interface->Type)
		{
		case 4:	conn->RemoteIP.v4 = *(tIPv4*)Address;	break;
		case 6:	conn->RemoteIP.v6 = *(tIPv6*)Address;	break;
		}
		
		conn->RecievedBuffer = RingBuffer_Create( TCP_RECIEVE_BUFFER_SIZE );
		
		conn->NextSequenceRcv = ntohl( hdr->SequenceNumber ) + 1;
		conn->NextSequenceSend = rand();
		
		// Create node
		conn->Node.NumACLs = 1;
		conn->Node.ACLs = &gVFS_ACL_EveryoneRW;
		conn->Node.ImplPtr = conn;
		conn->Node.ImplInt = srv->NextID ++;
		conn->Node.Type = &gTCP_ClientNodeType;	// TODO: Special type for the server end?
		
		// Hmm... Theoretically, this lock will never have to wait,
		// as the interface is locked to the watching thread, and this
		// runs in the watching thread. But, it's a good idea to have
		// it, just in case
		// Oh, wait, there is a case where a wildcard can be used
		// (srv->Interface == NULL) so having the lock is a good idea
		SHORTLOCK(&srv->lConnections);
		if( !srv->Connections )
			srv->Connections = conn;
		else
			srv->ConnectionsTail->Next = conn;
		srv->ConnectionsTail = conn;
		if(!srv->NewConnections)
			srv->NewConnections = conn;
		VFS_MarkAvaliable( &srv->Node, 1 );
		SHORTREL(&srv->lConnections);
		Semaphore_Signal(&srv->WaitingConnections, 1);

		// Send the SYN ACK
		hdr->Flags |= TCP_FLAG_ACK;
		hdr->AcknowlegementNumber = htonl(conn->NextSequenceRcv);
		hdr->SequenceNumber = htonl(conn->NextSequenceSend);
		hdr->DestPort = hdr->SourcePort;
		hdr->SourcePort = htons(srv->Port);
		hdr->DataOffset = (sizeof(tTCPHeader)/4) << 4;
		TCP_SendPacket( conn, hdr, 0, NULL );
		conn->NextSequenceSend ++;
		return ;
	}

	// Check Open Connections
	{
		for( conn = gTCP_OutbountCons; conn; conn = conn->Next )
		{
			// Check that it is coming in on the same interface
			if(conn->Interface != Interface)	continue;

			// Check Source Port
			if(conn->RemotePort != ntohs(hdr->SourcePort))	continue;

			// Check Source IP
			if(conn->Interface->Type == 6 && !IP6_EQU(conn->RemoteIP.v6, *(tIPv6*)Address))
				continue;
			if(conn->Interface->Type == 4 && !IP4_EQU(conn->RemoteIP.v4, *(tIPv4*)Address))
				continue;

			TCP_INT_HandleConnectionPacket(conn, hdr, Length);
			return ;
		}
	}
	
	Log_Log("TCP", "TCP_GetPacket: No Match");
}

/**
 * \brief Handles a packet sent to a specific connection
 * \param Connection	TCP Connection pointer
 * \param Header	TCP Packet pointer
 * \param Length	Length of the packet
 */
void TCP_INT_HandleConnectionPacket(tTCPConnection *Connection, tTCPHeader *Header, int Length)
{
	 int	dataLen;
	Uint32	sequence_num;
	
	// Silently drop once finished
	// TODO: Check if this needs to be here
	if( Connection->State == TCP_ST_FINISHED ) {
		Log_Log("TCP", "Packet ignored - connection finnished");
		return ;
	}
	
	// Syncronise sequence values
	if(Header->Flags & TCP_FLAG_SYN) {
		// TODO: What if the packet also has data?
		if( Connection->LastACKSequence != Connection->NextSequenceRcv )
			TCP_INT_SendACK(Connection);
		Connection->NextSequenceRcv = ntohl(Header->SequenceNumber);
		Connection->LastACKSequence = Connection->NextSequenceRcv;
	}
	
	// Ackowledge a sent packet
	if(Header->Flags & TCP_FLAG_ACK) {
		// TODO: Process an ACKed Packet
		LOG("Conn %p, Sent packet 0x%x ACKed", Connection, Header->AcknowlegementNumber);
	}
	
	// Get length of data
	dataLen = Length - (Header->DataOffset>>4)*4;
	LOG("dataLen = %i", dataLen);
	Log_Debug("TCP", "State %i, dataLen = %x", Connection->State, dataLen);
	
	// 
	// State Machine
	//
	switch( Connection->State )
	{
	// Pre-init connection?
	case TCP_ST_CLOSED:
		Log_Log("TCP", "Packets to a closed connection?!");
		break;
	
	// --- Init States ---
	// SYN sent, expecting SYN-ACK Connection Opening
	case TCP_ST_SYN_SENT:
		if( Header->Flags & TCP_FLAG_SYN )
		{
			Connection->NextSequenceRcv ++;
			
			if( Header->Flags & TCP_FLAG_ACK )
			{	
				Log_Log("TCP", "ACKing SYN-ACK");
				Connection->State = TCP_ST_OPEN;
				VFS_MarkFull(&Connection->Node, 0);
			}
			else
			{
				Log_Log("TCP", "ACKing SYN");
				Connection->State = TCP_ST_SYN_RCVD;
			}
			Header->DestPort = Header->SourcePort;
			Header->SourcePort = htons(Connection->LocalPort);
			Header->AcknowlegementNumber = htonl(Connection->NextSequenceRcv);
			Header->SequenceNumber = htonl(Connection->NextSequenceSend);
			Header->WindowSize = htons(TCP_WINDOW_SIZE);
			Header->Flags = TCP_FLAG_ACK;
			Header->DataOffset = (sizeof(tTCPHeader)/4) << 4;
			TCP_SendPacket( Connection, Header, 0, NULL );
		}
		break;
	
	// SYN-ACK sent, expecting ACK
	case TCP_ST_SYN_RCVD:
		if( Header->Flags & TCP_FLAG_ACK )
		{
			// TODO: Handle max half-open limit
			Log_Log("TCP", "Connection fully opened");
			Connection->State = TCP_ST_OPEN;
			VFS_MarkFull(&Connection->Node, 0);
		}
		break;
		
	// --- Established State ---
	case TCP_ST_OPEN:
		// - Handle State changes
		//
		if( Header->Flags & TCP_FLAG_FIN ) {
			Log_Log("TCP", "Conn %p closed, recieved FIN", Connection);
			VFS_MarkError(&Connection->Node, 1);
			Connection->State = TCP_ST_CLOSE_WAIT;
//			Header->Flags &= ~TCP_FLAG_FIN;
			// CLOSE WAIT requires the client to close (or does it?)
			#if 0
			
			#endif
		}
	
		// Check for an empty packet
		if(dataLen == 0) {
			if( Header->Flags == TCP_FLAG_ACK )
			{
				Log_Log("TCP", "ACK only packet");
				return ;
			}
			Connection->NextSequenceRcv ++;	// TODO: Is this right? (empty packet counts as one byte)
			Log_Log("TCP", "Empty Packet, inc and ACK the current sequence number");
			TCP_INT_SendACK(Connection);
			#if 0
			Header->DestPort = Header->SourcePort;
			Header->SourcePort = htons(Connection->LocalPort);
			Header->AcknowlegementNumber = htonl(Connection->NextSequenceRcv);
			Header->SequenceNumber = htonl(Connection->NextSequenceSend);
			Header->Flags |= TCP_FLAG_ACK;
			TCP_SendPacket( Connection, Header, 0, NULL );
			#endif
			return ;
		}
		
		// NOTES:
		// Flags
		//    PSH - Has Data?
		// /NOTES
		
		sequence_num = ntohl(Header->SequenceNumber);
		
		LOG("0x%08x <= 0x%08x < 0x%08x",
			Connection->NextSequenceRcv,
			ntohl(Header->SequenceNumber),
			Connection->NextSequenceRcv + TCP_WINDOW_SIZE
			);
		
		// Is this packet the next expected packet?
		if( sequence_num == Connection->NextSequenceRcv )
		{
			 int	rv;
			// Ooh, Goodie! Add it to the recieved list
			rv = TCP_INT_AppendRecieved(Connection,
				(Uint8*)Header + (Header->DataOffset>>4)*4,
				dataLen
				);
			if(rv != 0) {
				Log_Notice("TCP", "TCP_INT_AppendRecieved rv %i", rv);
				break;
			}
			LOG("0x%08x += %i", Connection->NextSequenceRcv, dataLen);
			Connection->NextSequenceRcv += dataLen;
			
			// TODO: This should be moved out of the watcher thread,
			// so that a single lost packet on one connection doesn't cause
			// all connections on the interface to lag.
			// - Meh, no real issue, as the cache shouldn't be that large
			TCP_INT_UpdateRecievedFromFuture(Connection);

			#if 1
			// - Only send an ACK if we've had a burst
			if( Connection->NextSequenceRcv > (Uint32)(TCP_DACK_THRESHOLD + Connection->LastACKSequence) )
			{
				TCP_INT_SendACK(Connection);
				// - Extend TCP deferred ACK timer
				Time_RemoveTimer(Connection->DeferredACKTimer);
			}
			// - Schedule the deferred ACK timer (if already scheduled, this is a NOP)
			Time_ScheduleTimer(Connection->DeferredACKTimer, TCP_DACK_TIMEOUT);
			#else
			TCP_INT_SendACK(Connection);
			#endif
		}
		// Check if the packet is in window
		else if( WrapBetween(Connection->NextSequenceRcv, sequence_num,
				Connection->NextSequenceRcv+TCP_WINDOW_SIZE, 0xFFFFFFFF) )
		{
			Uint8	*dataptr = (Uint8*)Header + (Header->DataOffset>>4)*4;
			#if CACHE_FUTURE_PACKETS_IN_BYTES
			Uint32	index;
			 int	i;
			
			index = sequence_num % TCP_WINDOW_SIZE;
			for( i = 0; i < dataLen; i ++ )
			{
				Connection->FuturePacketValidBytes[index/8] |= 1 << (index%8);
				Connection->FuturePacketData[index] = dataptr[i];
				// Do a wrap increment
				index ++;
				if(index == TCP_WINDOW_SIZE)	index = 0;
			}
			#else
			tTCPStoredPacket	*pkt, *tmp, *prev = NULL;
			
			// Allocate and fill cached packet
			pkt = malloc( sizeof(tTCPStoredPacket) + dataLen );
			pkt->Next = NULL;
			pkt->Sequence = ntohl(Header->SequenceNumber);
			pkt->Length = dataLen;
			memcpy(pkt->Data, dataptr, dataLen);
			
			Log_Log("TCP", "We missed a packet, caching",
				pkt->Sequence, Connection->NextSequenceRcv);
			
			// No? Well, let's cache it and look at it later
			SHORTLOCK( &Connection->lFuturePackets );
			for(tmp = Connection->FuturePackets;
				tmp;
				prev = tmp, tmp = tmp->Next)
			{
				if(tmp->Sequence >= pkt->Sequence)	break;
			}
			
			// Add if before first, or sequences don't match 
			if( !tmp || tmp->Sequence != pkt->Sequence )
			{
				if(prev)
					prev->Next = pkt;
				else
					Connection->FuturePackets = pkt;
				pkt->Next = tmp;
			}
			// Replace if larger
			else if(pkt->Length > tmp->Length)
			{
				if(prev)
					prev->Next = pkt;
				pkt->Next = tmp->Next;
				free(tmp);
			}
			else
			{
				free(pkt);	// TODO: Find some way to remove this
			}
			SHORTREL( &Connection->lFuturePackets );
			#endif
		}
		// Badly out of sequence packet
		else
		{
			Log_Log("TCP", "Fully out of sequence packet (0x%08x not between 0x%08x and 0x%08x), dropped",
				sequence_num, Connection->NextSequenceRcv, Connection->NextSequenceRcv+TCP_WINDOW_SIZE);
			// Spec says we should send an empty ACK with the current state
			TCP_INT_SendACK(Connection);
		}
		break;
	
	// --- Remote close states
	case TCP_ST_CLOSE_WAIT:
		
		// Ignore everything, CLOSE_WAIT is terminated by the client
		Log_Debug("TCP", "CLOSE WAIT - Ignoring packets");
		
		break;
	
	// LAST-ACK - Waiting for the ACK of FIN (from CLOSE WAIT)
	case TCP_ST_LAST_ACK:
		if( Header->Flags & TCP_FLAG_ACK )
		{
			Connection->State = TCP_ST_FINISHED;	// Connection completed
			Log_Log("TCP", "LAST-ACK to CLOSED - Connection remote closed");
			// TODO: Destrory the TCB
		}
		break;
	
	// --- Local close States
	case TCP_ST_FIN_WAIT1:
		if( Header->Flags & TCP_FLAG_FIN )
		{
			Connection->State = TCP_ST_CLOSING;
			Log_Debug("TCP", "Conn %p closed, sent FIN and recieved FIN", Connection);
			VFS_MarkError(&Connection->Node, 1);
			
			// ACK Packet
			Header->DestPort = Header->SourcePort;
			Header->SourcePort = htons(Connection->LocalPort);
			Header->AcknowlegementNumber = Header->SequenceNumber;
			Header->SequenceNumber = htonl(Connection->NextSequenceSend);
			Header->WindowSize = htons(TCP_WINDOW_SIZE);
			Header->Flags = TCP_FLAG_ACK;
			TCP_SendPacket( Connection, Header, 0, NULL );
			break ;
		}
		
		// TODO: Make sure that the packet is actually ACKing the FIN
		if( Header->Flags & TCP_FLAG_ACK )
		{
			Connection->State = TCP_ST_FIN_WAIT2;
			Log_Debug("TCP", "Conn %p closed, sent FIN ACKed", Connection);
			VFS_MarkError(&Connection->Node, 1);
			return ;
		}
		break;
	
	case TCP_ST_FIN_WAIT2:
		if( Header->Flags & TCP_FLAG_FIN )
		{
			Connection->State = TCP_ST_TIME_WAIT;
			Log_Debug("TCP", "FIN sent and recieved, ACKing and going into TIME WAIT %p FINWAIT-2 -> TIME WAIT", Connection);
			// Send ACK
			Header->DestPort = Header->SourcePort;
			Header->SourcePort = htons(Connection->LocalPort);
			Header->AcknowlegementNumber = Header->SequenceNumber;
			Header->SequenceNumber = htonl(Connection->NextSequenceSend);
			Header->WindowSize = htons(TCP_WINDOW_SIZE);
			Header->Flags = TCP_FLAG_ACK;
			TCP_SendPacket( Connection, Header, 0, NULL );
		}
		break;
	
	case TCP_ST_CLOSING:
		// TODO: Make sure that the packet is actually ACKing the FIN
		if( Header->Flags & TCP_FLAG_ACK )
		{
			Connection->State = TCP_ST_TIME_WAIT;
			Log_Debug("TCP", "Conn %p CLOSING -> TIME WAIT", Connection);
			VFS_MarkError(&Connection->Node, 1);
			return ;
		}
		break;
	
	// --- Closed (or near closed) states) ---
	case TCP_ST_TIME_WAIT:
		Log_Log("TCP", "Packets on Time-Wait, ignored");
		break;
	
	case TCP_ST_FINISHED:
		Log_Log("TCP", "Packets when CLOSED, ignoring");
		break;
	
	//default:
	//	Log_Warning("TCP", "Unhandled TCP state %i", Connection->State);
	//	break;
	}
	
}

/**
 * \brief Appends a packet to the recieved list
 * \param Connection	Connection structure
 * \param Data	Packet contents
 * \param Length	Length of \a Data
 */
int TCP_INT_AppendRecieved(tTCPConnection *Connection, const void *Data, size_t Length)
{
	Mutex_Acquire( &Connection->lRecievedPackets );

	if(Connection->RecievedBuffer->Length + Length > Connection->RecievedBuffer->Space )
	{
		VFS_MarkAvaliable(&Connection->Node, 1);
		Log_Error("TCP", "Buffer filled, packet dropped (:%i) - %i + %i > %i",
			Connection->LocalPort, Connection->RecievedBuffer->Length, Length,
			Connection->RecievedBuffer->Space
			);
		Mutex_Release( &Connection->lRecievedPackets );
		return 1;
	}
	
	RingBuffer_Write( Connection->RecievedBuffer, Data, Length );

	VFS_MarkAvaliable(&Connection->Node, 1);
	
	Mutex_Release( &Connection->lRecievedPackets );
	return 0;
}

/**
 * \brief Updates the connections recieved list from the future list
 * \param Connection	Connection structure
 * 
 * Updates the recieved packets list with packets from the future (out 
 * of order) packets list that are now able to be added in direct
 * sequence.
 */
void TCP_INT_UpdateRecievedFromFuture(tTCPConnection *Connection)
{
	#if CACHE_FUTURE_PACKETS_IN_BYTES
	 int	i, length = 0;
	Uint32	index;
	
	// Calculate length of contiguous bytes
	length = Connection->HighestSequenceRcvd - Connection->NextSequenceRcv;
	index = Connection->NextSequenceRcv % TCP_WINDOW_SIZE;
	for( i = 0; i < length; i ++ )
	{
		if( Connection->FuturePacketValidBytes[i / 8] == 0xFF ) {
			i += 7;	index += 7;
			continue;
		}
		else if( !(Connection->FuturePacketValidBytes[i / 8] & (1 << (i%8))) )
			break;
		
		index ++;
		if(index > TCP_WINDOW_SIZE)
			index -= TCP_WINDOW_SIZE;
	}
	length = i;
	
	index = Connection->NextSequenceRcv % TCP_WINDOW_SIZE;
	
	// Write data to to the ring buffer
	if( TCP_WINDOW_SIZE - index > length )
	{
		// Simple case
		RingBuffer_Write( Connection->RecievedBuffer, Connection->FuturePacketData + index, length );
	}
	else
	{
		 int	endLen = TCP_WINDOW_SIZE - index;
		// 2-part case
		RingBuffer_Write( Connection->RecievedBuffer, Connection->FuturePacketData + index, endLen );
		RingBuffer_Write( Connection->RecievedBuffer, Connection->FuturePacketData, endLen - length );
	}
	
	// Mark (now saved) bytes as invalid
	// - Align index
	while(index % 8 && length)
	{
		Connection->FuturePacketData[index] = 0;
		Connection->FuturePacketData[index/8] &= ~(1 << (index%8));
		index ++;
		if(index > TCP_WINDOW_SIZE)
			index -= TCP_WINDOW_SIZE;
		length --;
	}
	while( length > 7 )
	{
		Connection->FuturePacketData[index] = 0;
		Connection->FuturePacketValidBytes[index/8] = 0;
		length -= 8;
		index += 8;
		if(index > TCP_WINDOW_SIZE)
			index -= TCP_WINDOW_SIZE;
	}
	while(length)
	{
		Connection->FuturePacketData[index] = 0;
		Connection->FuturePacketData[index/8] &= ~(1 << (index%8));
		index ++;
		if(index > TCP_WINDOW_SIZE)
			index -= TCP_WINDOW_SIZE;
		length --;
	}
	
	#else
	tTCPStoredPacket	*pkt;
	for(;;)
	{
		SHORTLOCK( &Connection->lFuturePackets );
		
		// Clear out duplicates from cache
		// - If a packet has just been recieved, and it is expected, then
		//   (since NextSequenceRcv = rcvd->Sequence + rcvd->Length) all
		//   packets in cache that are smaller than the next expected
		//   are now defunct.
		pkt = Connection->FuturePackets;
		while(pkt && pkt->Sequence < Connection->NextSequenceRcv)
		{
			tTCPStoredPacket	*next = pkt->Next;
			free(pkt);
			pkt = next;
		}
		
		// If there's no packets left in cache, stop looking
		if(!pkt || pkt->Sequence > Connection->NextSequenceRcv) {
			SHORTREL( &Connection->lFuturePackets );
			return;
		}
		
		// Delete packet from future list
		Connection->FuturePackets = pkt->Next;
		
		// Release list
		SHORTREL( &Connection->lFuturePackets );
		
		// Looks like we found one
		TCP_INT_AppendRecieved(Connection, pkt);
		Connection->NextSequenceRcv += pkt->Length;
		free(pkt);
	}
	#endif
}

void TCP_INT_SendACK(tTCPConnection *Connection)
{
	tTCPHeader	hdr;
	// ACK Packet
	hdr.DataOffset = (sizeof(tTCPHeader)/4) << 4;
	hdr.DestPort = htons(Connection->RemotePort);
	hdr.SourcePort = htons(Connection->LocalPort);
	hdr.AcknowlegementNumber = htonl(Connection->NextSequenceRcv);
	hdr.SequenceNumber = htonl(Connection->NextSequenceSend);
	hdr.WindowSize = htons(TCP_WINDOW_SIZE);
	hdr.Flags = TCP_FLAG_ACK;	// TODO: Determine if SYN is wanted too
	hdr.Checksum = 0;	// TODO: Checksum
	hdr.UrgentPointer = 0;
	Log_Debug("TCP", "Sending ACK for 0x%08x", Connection->NextSequenceRcv);
	TCP_SendPacket( Connection, &hdr, 0, NULL );
	//Connection->NextSequenceSend ++;
	Connection->LastACKSequence = Connection->NextSequenceRcv;
}

/**
 * \fn Uint16 TCP_GetUnusedPort()
 * \brief Gets an unused port and allocates it
 */
Uint16 TCP_GetUnusedPort()
{
	Uint16	ret;

	// Get Next outbound port
	ret = giTCP_NextOutPort++;
	while( gaTCP_PortBitmap[ret/32] & (1UL << (ret%32)) )
	{
		ret ++;
		giTCP_NextOutPort++;
		if(giTCP_NextOutPort == 0x10000) {
			ret = giTCP_NextOutPort = TCP_MIN_DYNPORT;
		}
	}

	// Mark the new port as used
	gaTCP_PortBitmap[ret/32] |= 1 << (ret%32);

	return ret;
}

/**
 * \fn int TCP_AllocatePort(Uint16 Port)
 * \brief Marks a port as used
 */
int TCP_AllocatePort(Uint16 Port)
{
	// Check if the port has already been allocated
	if( gaTCP_PortBitmap[Port/32] & (1 << (Port%32)) )
		return 0;

	// Allocate
	gaTCP_PortBitmap[Port/32] |= 1 << (Port%32);

	return 1;
}

/**
 * \fn int TCP_DeallocatePort(Uint16 Port)
 * \brief Marks a port as unused
 */
int TCP_DeallocatePort(Uint16 Port)
{
	// Check if the port has already been allocated
	if( !(gaTCP_PortBitmap[Port/32] & (1 << (Port%32))) )
		return 0;

	// Allocate
	gaTCP_PortBitmap[Port/32] &= ~(1 << (Port%32));

	return 1;
}

// --- Server
tVFS_Node *TCP_Server_Init(tInterface *Interface)
{
	tTCPListener	*srv;
	
	srv = calloc( 1, sizeof(tTCPListener) );

	if( srv == NULL ) {
		Log_Warning("TCP", "malloc failed for listener (%i) bytes", sizeof(tTCPListener));
		return NULL;
	}

	srv->Interface = Interface;
	srv->Port = 0;
	srv->NextID = 0;
	srv->Connections = NULL;
	srv->ConnectionsTail = NULL;
	srv->NewConnections = NULL;
	srv->Next = NULL;
	srv->Node.Flags = VFS_FFLAG_DIRECTORY;
	srv->Node.Size = -1;
	srv->Node.ImplPtr = srv;
	srv->Node.NumACLs = 1;
	srv->Node.ACLs = &gVFS_ACL_EveryoneRW;
	srv->Node.Type = &gTCP_ServerNodeType;

	SHORTLOCK(&glTCP_Listeners);
	srv->Next = gTCP_Listeners;
	gTCP_Listeners = srv;
	SHORTREL(&glTCP_Listeners);

	return &srv->Node;
}

/**
 * \brief Wait for a new connection and return the connection ID
 * \note Blocks until a new connection is made
 * \param Node	Server node
 * \param Pos	Position (ignored)
 */
int TCP_Server_ReadDir(tVFS_Node *Node, int Pos, char Dest[FILENAME_MAX])
{
	tTCPListener	*srv = Node->ImplPtr;
	tTCPConnection	*conn;
	
	ENTER("pNode iPos", Node, Pos);

	Log_Log("TCP", "Thread %i waiting for a connection", Threads_GetTID());
	Semaphore_Wait( &srv->WaitingConnections, 1 );
	
	SHORTLOCK(&srv->lConnections);
	// Increment the new list (the current connection is still on the 
	// normal list)
	conn = srv->NewConnections;
	srv->NewConnections = conn->Next;

	if( srv->NewConnections == NULL )
		VFS_MarkAvaliable( Node, 0 );
	
	SHORTREL( &srv->lConnections );
	
	LOG("conn = %p", conn);
	LOG("srv->Connections = %p", srv->Connections);
	LOG("srv->NewConnections = %p", srv->NewConnections);
	LOG("srv->ConnectionsTail = %p", srv->ConnectionsTail);

	itoa(Dest, conn->Node.ImplInt, 16, 8, '0');
	Log_Log("TCP", "Thread %i got connection '%s'", Threads_GetTID(), Dest);
	LEAVE('i', 0);
	return 0;
}

/**
 * \brief Gets a client connection node
 * \param Node	Server node
 * \param Name	Hexadecimal ID of the node
 */
tVFS_Node *TCP_Server_FindDir(tVFS_Node *Node, const char *Name, Uint Flags)
{
	tTCPConnection	*conn;
	tTCPListener	*srv = Node->ImplPtr;
	char	tmp[9];
	 int	id = atoi(Name);
	
	ENTER("pNode sName", Node, Name);

	// Check for a non-empty name
	if( Name[0] ) 
	{	
		// Sanity Check
		itoa(tmp, id, 16, 8, '0');
		if(strcmp(tmp, Name) != 0) {
			LOG("'%s' != '%s' (%08x)", Name, tmp, id);
			LEAVE('n');
			return NULL;
		}
		
		Log_Debug("TCP", "srv->Connections = %p", srv->Connections);
		Log_Debug("TCP", "srv->NewConnections = %p", srv->NewConnections);
		Log_Debug("TCP", "srv->ConnectionsTail = %p", srv->ConnectionsTail);
		
		// Search
		SHORTLOCK( &srv->lConnections );
		for(conn = srv->Connections;
			conn;
			conn = conn->Next)
		{
			LOG("conn->Node.ImplInt = %i", conn->Node.ImplInt);
			if(conn->Node.ImplInt == id)	break;
		}
		SHORTREL( &srv->lConnections );

		// If not found, ret NULL
		if(!conn) {
			LOG("Connection %i not found", id);
			LEAVE('n');
			return NULL;
		}
	}
	// Empty Name - Check for a new connection and if it's there, open it
	else
	{
		SHORTLOCK( &srv->lConnections );
		conn = srv->NewConnections;
		if( conn != NULL )
			srv->NewConnections = conn->Next;
		VFS_MarkAvaliable( Node, srv->NewConnections != NULL );
		SHORTREL( &srv->lConnections );
		if( !conn ) {
			LOG("No new connections");
			LEAVE('n');
			return NULL;
		}
	}
		
	// Return node
	LEAVE('p', &conn->Node);
	return &conn->Node;
}

/**
 * \brief Handle IOCtl calls
 */
int TCP_Server_IOCtl(tVFS_Node *Node, int ID, void *Data)
{
	tTCPListener	*srv = Node->ImplPtr;

	switch(ID)
	{
	case 4:	// Get/Set Port
		if(!Data)	// Get Port
			return srv->Port;

		if(srv->Port)	// Wait, you can't CHANGE the port
			return -1;

		if(!CheckMem(Data, sizeof(Uint16)))	// Sanity check
			return -1;

		// Permissions check
		if(Threads_GetUID() != 0
		&& *(Uint16*)Data != 0
		&& *(Uint16*)Data < 1024)
			return -1;

		// TODO: Check if a port is in use

		// Set Port
		srv->Port = *(Uint16*)Data;
		if(srv->Port == 0)	// Allocate a random port
			srv->Port = TCP_GetUnusedPort();
		else	// Else, mark this as used
			TCP_AllocatePort(srv->Port);
		
		Log_Log("TCP", "Server %p listening on port %i", srv, srv->Port);
		
		return srv->Port;
	}
	return 0;
}

void TCP_Server_Close(tVFS_Node *Node)
{
	free(Node->ImplPtr);
}

// --- Client
/**
 * \brief Create a client node
 */
tVFS_Node *TCP_Client_Init(tInterface *Interface)
{
	tTCPConnection	*conn = calloc( sizeof(tTCPConnection) + TCP_WINDOW_SIZE + TCP_WINDOW_SIZE/8, 1 );

	conn->State = TCP_ST_CLOSED;
	conn->Interface = Interface;
	conn->LocalPort = -1;
	conn->RemotePort = -1;

	conn->Node.ImplPtr = conn;
	conn->Node.NumACLs = 1;
	conn->Node.ACLs = &gVFS_ACL_EveryoneRW;
	conn->Node.Type = &gTCP_ClientNodeType;
	conn->Node.BufferFull = 1;	// Cleared when connection opens

	conn->RecievedBuffer = RingBuffer_Create( TCP_RECIEVE_BUFFER_SIZE );
	#if 0
	conn->SentBuffer = RingBuffer_Create( TCP_SEND_BUFFER_SIZE );
	Semaphore_Init(conn->SentBufferSpace, 0, TCP_SEND_BUFFER_SIZE, "TCP SentBuffer", conn->Name);
	#endif
	
	#if CACHE_FUTURE_PACKETS_IN_BYTES
	// Future recieved data (ahead of the expected sequence number)
	conn->FuturePacketData = (Uint8*)conn + sizeof(tTCPConnection);
	conn->FuturePacketValidBytes = conn->FuturePacketData + TCP_WINDOW_SIZE;
	#endif

	conn->DeferredACKTimer = Time_AllocateTimer( (void(*)(void*)) TCP_INT_SendACK, conn);

	SHORTLOCK(&glTCP_OutbountCons);
	conn->Next = gTCP_OutbountCons;
	gTCP_OutbountCons = conn;
	SHORTREL(&glTCP_OutbountCons);

	return &conn->Node;
}

/**
 * \brief Wait for a packet and return it
 * \note If \a Length is smaller than the size of the packet, the rest
 *       of the packet's data will be discarded.
 */
size_t TCP_Client_Read(tVFS_Node *Node, off_t Offset, size_t Length, void *Buffer, Uint Flags)
{
	tTCPConnection	*conn = Node->ImplPtr;
	size_t	len;
	
	ENTER("pNode XOffset XLength pBuffer", Node, Offset, Length, Buffer);
	LOG("conn = %p {State:%i}", conn, conn->State);
	
	// If the connection has been closed (state > ST_OPEN) then clear
	// any stale data in the buffer (until it is empty (until it is empty))
	if( conn->State > TCP_ST_OPEN )
	{
		Mutex_Acquire( &conn->lRecievedPackets );
		len = RingBuffer_Read( Buffer, conn->RecievedBuffer, Length );
		Mutex_Release( &conn->lRecievedPackets );
		
		if( len == 0 ) {
			VFS_MarkAvaliable(Node, 0);
			errno = 0;
			LEAVE('i', -1);
			return -1;
		}
		
		LEAVE('i', len);
		return len;
	}
	
	// Wait
	{
		tTime	*timeout = NULL;
		tTime	timeout_zero = 0;
		if( Flags & VFS_IOFLAG_NOBLOCK )
			timeout = &timeout_zero;
		if( !VFS_SelectNode(Node, VFS_SELECT_READ|VFS_SELECT_ERROR, timeout, "TCP_Client_Read") ) {
			errno = EWOULDBLOCK;
			LEAVE('i', -1);
			return -1;
		}
	}
	
	// Lock list and read as much as possible (up to `Length`)
	Mutex_Acquire( &conn->lRecievedPackets );
	len = RingBuffer_Read( Buffer, conn->RecievedBuffer, Length );
	
	if( len == 0 || conn->RecievedBuffer->Length == 0 ) {
		LOG("Marking as none avaliable (len = %i)", len);
		VFS_MarkAvaliable(Node, 0);
	}
		
	// Release the lock (we don't need it any more)
	Mutex_Release( &conn->lRecievedPackets );

	LEAVE('i', len);
	return len;
}

/**
 * \brief Send a data packet on a connection
 */
void TCP_INT_SendDataPacket(tTCPConnection *Connection, size_t Length, const void *Data)
{
	char	buf[sizeof(tTCPHeader)+Length];
	tTCPHeader	*packet = (void*)buf;
	
	packet->SourcePort = htons(Connection->LocalPort);
	packet->DestPort = htons(Connection->RemotePort);
	packet->DataOffset = (sizeof(tTCPHeader)/4)*16;
	packet->WindowSize = htons(TCP_WINDOW_SIZE);
	
	packet->AcknowlegementNumber = htonl(Connection->NextSequenceRcv);
	packet->SequenceNumber = htonl(Connection->NextSequenceSend);
	packet->Flags = TCP_FLAG_PSH|TCP_FLAG_ACK;	// Hey, ACK if you can!
	
	memcpy(packet->Options, Data, Length);
	
	Log_Debug("TCP", "Send sequence 0x%08x", Connection->NextSequenceSend);
#if HEXDUMP_OUTGOING
	Debug_HexDump("TCP_INT_SendDataPacket: Data = ", Data, Length);
#endif
	
	TCP_SendPacket( Connection, packet, Length, Data );
	
	Connection->NextSequenceSend += Length;
}

/**
 * \brief Send some bytes on a connection
 */
size_t TCP_Client_Write(tVFS_Node *Node, off_t Offset, size_t Length, const void *Buffer, Uint Flags)
{
	tTCPConnection	*conn = Node->ImplPtr;
	size_t	rem = Length;
	
	ENTER("pNode XOffset XLength pBuffer", Node, Offset, Length, Buffer);
	
//	#if DEBUG
//	Debug_HexDump("TCP_Client_Write: Buffer = ",
//		Buffer, Length);
//	#endif
	
	// Don't allow a write to a closed connection
	if( conn->State > TCP_ST_OPEN ) {
		VFS_MarkError(Node, 1);
		errno = 0;
		LEAVE('i', -1);
		return -1;
	}
	
	// Wait
	{
		tTime	*timeout = NULL;
		tTime	timeout_zero = 0;
		if( Flags & VFS_IOFLAG_NOBLOCK )
			timeout = &timeout_zero;
		if( !VFS_SelectNode(Node, VFS_SELECT_WRITE|VFS_SELECT_ERROR, timeout, "TCP_Client_Write") ) {
			errno = EWOULDBLOCK;
			LEAVE('i', -1);
			return -1;
		}
	}
	
	do
	{
		 int	len = (rem < TCP_MAX_PACKET_SIZE) ? rem : TCP_MAX_PACKET_SIZE;
		
		#if 0
		// Wait for space in the buffer
		Semaphore_Signal( &Connection->SentBufferSpace, len );
		
		// Save data to buffer (and update the length read by the ammount written)
		len = RingBuffer_Write( &Connection->SentBuffer, Buffer, len);
		#endif
		
		// Send packet
		TCP_INT_SendDataPacket(conn, len, Buffer);
		
		Buffer += len;
		rem -= len;
	} while( rem > 0 );
	
	LEAVE('i', Length);
	return Length;
}

/**
 * \brief Open a connection to another host using TCP
 * \param Conn	Connection structure
 */
void TCP_StartConnection(tTCPConnection *Conn)
{
	tTCPHeader	hdr = {0};

	Conn->State = TCP_ST_SYN_SENT;

	hdr.SourcePort = htons(Conn->LocalPort);
	hdr.DestPort = htons(Conn->RemotePort);
	Conn->NextSequenceSend = rand();
	hdr.SequenceNumber = htonl(Conn->NextSequenceSend);
	hdr.DataOffset = (sizeof(tTCPHeader)/4) << 4;
	hdr.Flags = TCP_FLAG_SYN;
	hdr.WindowSize = htons(TCP_WINDOW_SIZE);	// Max
	hdr.Checksum = 0;	// TODO
	
	TCP_SendPacket( Conn, &hdr, 0, NULL );
	
	Conn->NextSequenceSend ++;
	Conn->State = TCP_ST_SYN_SENT;

	return ;
}

/**
 * \brief Control a client socket
 */
int TCP_Client_IOCtl(tVFS_Node *Node, int ID, void *Data)
{
	tTCPConnection	*conn = Node->ImplPtr;
	
	ENTER("pNode iID pData", Node, ID, Data);

	switch(ID)
	{
	case 4:	// Get/Set local port
		if(!Data)
			LEAVE_RET('i', conn->LocalPort);
		if(conn->State != TCP_ST_CLOSED)
			LEAVE_RET('i', -1);
		if(!CheckMem(Data, sizeof(Uint16)))
			LEAVE_RET('i', -1);

		if(Threads_GetUID() != 0 && *(Uint16*)Data < 1024)
			LEAVE_RET('i', -1);

		conn->LocalPort = *(Uint16*)Data;
		LEAVE_RET('i', conn->LocalPort);

	case 5:	// Get/Set remote port
		if(!Data)	LEAVE_RET('i', conn->RemotePort);
		if(conn->State != TCP_ST_CLOSED)	LEAVE_RET('i', -1);
		if(!CheckMem(Data, sizeof(Uint16)))	LEAVE_RET('i', -1);
		conn->RemotePort = *(Uint16*)Data;
		LEAVE_RET('i', conn->RemotePort);

	case 6:	// Set Remote IP
		if( conn->State != TCP_ST_CLOSED )
			LEAVE_RET('i', -1);
		if( conn->Interface->Type == 4 )
		{
			if(!CheckMem(Data, sizeof(tIPv4)))	LEAVE_RET('i', -1);
			conn->RemoteIP.v4 = *(tIPv4*)Data;
		}
		else if( conn->Interface->Type == 6 )
		{
			if(!CheckMem(Data, sizeof(tIPv6)))	LEAVE_RET('i', -1);
			conn->RemoteIP.v6 = *(tIPv6*)Data;
		}
		LEAVE_RET('i', 0);

	case 7:	// Connect
		if(conn->LocalPort == 0xFFFF)
			conn->LocalPort = TCP_GetUnusedPort();
		if(conn->RemotePort == -1)
			LEAVE_RET('i', 0);

		{
			tTime	timeout = conn->Interface->TimeoutDelay;
	
			TCP_StartConnection(conn);
			VFS_SelectNode(&conn->Node, VFS_SELECT_WRITE, &timeout, "TCP Connection");
			if( conn->State == TCP_ST_SYN_SENT )
				LEAVE_RET('i', 0);
		}

		LEAVE_RET('i', 1);
	
	// Get recieve buffer length
	case 8:
		LEAVE_RET('i', conn->RecievedBuffer->Length);
	}

	return 0;
}

void TCP_Client_Close(tVFS_Node *Node)
{
	tTCPConnection	*conn = Node->ImplPtr;
	tTCPHeader	packet;
	
	ENTER("pNode", Node);
	
	if( conn->State == TCP_ST_CLOSE_WAIT || conn->State == TCP_ST_OPEN )
	{
		packet.SourcePort = htons(conn->LocalPort);
		packet.DestPort = htons(conn->RemotePort);
		packet.DataOffset = (sizeof(tTCPHeader)/4)*16;
		packet.WindowSize = TCP_WINDOW_SIZE;
		
		packet.AcknowlegementNumber = 0;
		packet.SequenceNumber = htonl(conn->NextSequenceSend);
		packet.Flags = TCP_FLAG_FIN;
		
		TCP_SendPacket( conn, &packet, 0, NULL );
	}
	
	switch( conn->State )
	{
	case TCP_ST_CLOSE_WAIT:
		conn->State = TCP_ST_LAST_ACK;
		break;
	case TCP_ST_OPEN:
		conn->State = TCP_ST_FIN_WAIT1;
		while( conn->State == TCP_ST_FIN_WAIT1 )	Threads_Yield();
		break;
	default:
		Log_Warning("TCP", "Unhandled connection state %i in TCP_Client_Close",
			conn->State);
		break;
	}
	
	Time_RemoveTimer(conn->DeferredACKTimer);
	Time_FreeTimer(conn->DeferredACKTimer);
	free(conn);
	
	LEAVE('-');
}

/**
 * \brief Checks if a value is between two others (after taking into account wrapping)
 */
int WrapBetween(Uint32 Lower, Uint32 Value, Uint32 Higher, Uint32 MaxValue)
{
	if( MaxValue < 0xFFFFFFFF )
	{
		Lower %= MaxValue + 1;
		Value %= MaxValue + 1;
		Higher %= MaxValue + 1;
	}
	
	// Simple Case, no wrap ?
	//       Lower Value Higher
	// | ... + ... + ... + ... |

	if( Lower < Higher ) {
		return Lower < Value && Value < Higher;
	}
	// Higher has wrapped below lower
	
	// Value > Lower ?
	//       Higher Lower Value
	// | ... +  ... + ... + ... |
	if( Value > Lower ) {
		return 1;
	}
	
	// Value < Higher ?
	//       Value Higher Lower
	// | ... + ... +  ... + ... |
	if( Value < Higher ) {
		return 1;
	}
	
	return 0;
}
