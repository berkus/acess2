/*
 * Acess2 IP Stack
 * - Address Resolution Protocol
 * - Part of the IPv4 protocol
 */
#define DEBUG	0
#include "ipstack.h"
#include "arp.h"
#include "link.h"
#include "ipv4.h"	// For IPv4_Netmask
#include "include/adapters_int.h"	// for MAC addr
#include <semaphore.h>
#include <timers.h>

#define ARPv6	0
#define	ARP_CACHE_SIZE	128
#define	ARP_MAX_AGE		(60*60*1000)	// 1Hr

// === IMPORTS ===
extern tInterface	*IPv4_GetInterface(tAdapter *Adapter, tIPv4 Address, int Broadcast);
#if ARPv6
extern tInterface	*IPv6_GetInterface(tAdapter *Adapter, tIPv6 Address, int Broadcast);
#endif

// === PROTOTYPES ===
 int	ARP_Initialise();
tMacAddr	ARP_Resolve4(tInterface *Interface, tIPv4 Address);
void	ARP_int_GetPacket(tAdapter *Adapter, tMacAddr From, int Length, void *Buffer);

// === GLOBALS ===
struct sARP_Cache4 {
	tIPv4	IP;
	tMacAddr	MAC;
	Sint64	LastUpdate;
	Sint64	LastUsed;
}	*gaARP_Cache4;
 int	giARP_Cache4Space;
tMutex	glARP_Cache4;
tSemaphore	gARP_Cache4Semaphore;
 int	giARP_WaitingThreads;
#if ARPv6
struct sARP_Cache6 {
	tIPv6	IP;
	tMacAddr	MAC;
	Sint64	LastUpdate;
	Sint64	LastUsed;
}	*gaARP_Cache6;
 int	giARP_Cache6Space;
tMutex	glARP_Cache6;
#endif

// === CODE ===
/**
 * \fn int ARP_Initialise()
 * \brief Initalise the ARP section
 */
int ARP_Initialise()
{
	gaARP_Cache4 = malloc( ARP_CACHE_SIZE * sizeof(struct sARP_Cache4) );
	memset( gaARP_Cache4, 0, ARP_CACHE_SIZE * sizeof(struct sARP_Cache4) );
	giARP_Cache4Space = ARP_CACHE_SIZE;
	
	#if ARPv6
	gaARP_Cache6 = malloc( ARP_CACHE_SIZE * sizeof(struct sARP_Cache6) );
	memset( gaARP_Cache6, 0, ARP_CACHE_SIZE * sizeof(struct sARP_Cache6) );
	giARP_Cache6Space = ARP_CACHE_SIZE;
	#endif
	
	Link_RegisterType(0x0806, ARP_int_GetPacket);
	Semaphore_Init(&gARP_Cache4Semaphore, 0, 0, "ARP4", "Cache Changes");
	return 1;
}

/**
 * \brief Resolves a MAC address from an IPv4 address
 */
tMacAddr ARP_Resolve4(tInterface *Interface, tIPv4 Address)
{
	 int	i;
	struct sArpRequest4	req;
	
	ENTER("pInterface xAddress", Interface, Address);
	
	// Check for broadcast
	if( Address.L == -1 )
	{
		LOG("Broadcast");
		LEAVE('-');
		return cMAC_BROADCAST;
	}

	// Check routing tables if not on this subnet
	if( IPStack_CompareAddress(4, &Address, Interface->Address, Interface->SubnetBits) == 0 )
	{
		tRoute	*route = IPStack_FindRoute(4, Interface, &Address);
		// If the next hop is defined, use it
		// - 0.0.0.0 as the next hop means "no next hop / direct"
		if( route && ((tIPv4*)route->NextHop)->L != 0 )
		{
			// Recursion: see /Recursion/
			LOG("Recursing with %s", IPStack_PrintAddress(4, route->NextHop));
			LEAVE('-');
			return ARP_Resolve4(Interface, *(tIPv4*)route->NextHop);
		}
		// No route, fall though
	}
	else
	{
		Uint32	netmask;
		// Check for broadcast
		netmask = IPv4_Netmask(Interface->SubnetBits);
		if( (Address.L & ~netmask) == (0xFFFFFFFF & ~netmask) )
		{
			LOG("Local Broadcast");
			LEAVE('-');
			return cMAC_BROADCAST;
		}
	}
	
	// Check ARP Cache
	Mutex_Acquire( &glARP_Cache4 );
	for( i = 0; i < giARP_Cache4Space; i++ )
	{
		if(gaARP_Cache4[i].IP.L != Address.L)	continue;
		
		// Check if the entry needs to be refreshed
		if( now() - gaARP_Cache4[i].LastUpdate > ARP_MAX_AGE )	break;
		
		Mutex_Release( &glARP_Cache4 );
		LOG("Return %x:%x:%x:%x:%x:%x",
			gaARP_Cache4[i].MAC.B[0], gaARP_Cache4[i].MAC.B[1],
			gaARP_Cache4[i].MAC.B[2], gaARP_Cache4[i].MAC.B[3],
			gaARP_Cache4[i].MAC.B[4], gaARP_Cache4[i].MAC.B[5]
			);
		LEAVE('-');
		return gaARP_Cache4[i].MAC;
	}
	giARP_WaitingThreads ++;
	Mutex_Release( &glARP_Cache4 );
	
	// Create request
	Log_Log("ARP4", "Asking for address %i.%i.%i.%i",
		Address.B[0], Address.B[1], Address.B[2], Address.B[3]
		);
	req.HWType = htons(0x0001);	// Ethernet
	req.Type   = htons(0x0800);
	req.HWSize = 6;
	req.SWSize = 4;
	req.Request = htons(1);
	memcpy(&req.SourceMac, Interface->Adapter->HWAddr, 6);	// TODO: Remove hard size
	req.SourceIP = *(tIPv4*)Interface->Address;
	req.DestMac = cMAC_BROADCAST;
	req.DestIP = Address;

	// Assumes only a header and footer at link layer
	tIPStackBuffer	*buffer = IPStack_Buffer_CreateBuffer(3);
	IPStack_Buffer_AppendSubBuffer(buffer, sizeof(struct sArpRequest4), 0, &req, NULL, NULL);

	// Send Request
	Link_SendPacket(Interface->Adapter, 0x0806, req.DestMac, buffer);

	// Clean up
	IPStack_Buffer_DestroyBuffer(buffer);
	
	// Wait for a reply
	Time_ScheduleTimer(NULL, Interface->TimeoutDelay);
	for(;;)
	{
		if( Semaphore_Wait(&gARP_Cache4Semaphore, 1) != 1 )
		{
			giARP_WaitingThreads --;
			Log_Log("ARP4", "Timeout");
			break;
		}
		Log_Debug("ARP4", "Cache change");
		
		Mutex_Acquire( &glARP_Cache4 );
		for( i = 0; i < giARP_Cache4Space; i++ )
		{
			if(gaARP_Cache4[i].IP.L != Address.L)	continue;
			
			giARP_WaitingThreads --;
			Mutex_Release( &glARP_Cache4 );
			Log_Debug("ARP4", "Return %02x:%02x:%02x:%02x:%02x:%02x",
				gaARP_Cache4[i].MAC.B[0], gaARP_Cache4[i].MAC.B[1], 
				gaARP_Cache4[i].MAC.B[2], gaARP_Cache4[i].MAC.B[3], 
				gaARP_Cache4[i].MAC.B[4], gaARP_Cache4[i].MAC.B[5]);
			return gaARP_Cache4[i].MAC;
		}
		Mutex_Release( &glARP_Cache4 );
	}
	{
		tMacAddr	ret = {{0,0,0,0,0,0}};
		return ret;
	}
}

/**
 * \brief Updates the ARP Cache entry for an IPv4 Address
 */
void ARP_UpdateCache4(tIPv4 SWAddr, tMacAddr HWAddr)
{
	 int	i;
	 int	free = -1;
	 int	oldest = 0;
	
	// Find an entry for the IP address in the cache
	Mutex_Acquire(&glARP_Cache4);
	for( i = giARP_Cache4Space; i--; )
	{
		if(gaARP_Cache4[oldest].LastUpdate > gaARP_Cache4[i].LastUpdate) {
			oldest = i;
		}
		if( gaARP_Cache4[i].IP.L == SWAddr.L )	break;
		if( gaARP_Cache4[i].LastUpdate == 0 && free == -1 )	free = i;
	}
	// If there was no match, we need to make one
	if(i == -1) {
		if(free != -1)
			i = free;
		else
			i = oldest;
	}
	
	Log_Log("ARP4", "Caching %i.%i.%i.%i (%02x:%02x:%02x:%02x:%02x:%02x) in %i",
		SWAddr.B[0], SWAddr.B[1], SWAddr.B[2], SWAddr.B[3],
		HWAddr.B[0], HWAddr.B[1], HWAddr.B[2], HWAddr.B[3], HWAddr.B[4], HWAddr.B[5],
		i
		);
		
	gaARP_Cache4[i].IP = SWAddr;
	gaARP_Cache4[i].MAC = HWAddr;
	gaARP_Cache4[i].LastUpdate = now();
	Semaphore_Signal(&gARP_Cache4Semaphore, giARP_WaitingThreads);
	Mutex_Release(&glARP_Cache4);
}

#if ARPv6
/**
 * \brief Updates the ARP Cache entry for an IPv6 Address
 */
void ARP_UpdateCache6(tIPv6 SWAddr, tMacAddr HWAddr)
{
	 int	i;
	 int	free = -1;
	 int	oldest = 0;
	
	// Find an entry for the MAC address in the cache
	Mutex_Acquire(&glARP_Cache6);
	for( i = giARP_Cache6Space; i--; )
	{
		if(gaARP_Cache6[oldest].LastUpdate > gaARP_Cache6[i].LastUpdate) {
			oldest = i;
		}
		if( MAC_EQU(gaARP_Cache6[i].MAC, HWAddr) )	break;
		if( gaARP_Cache6[i].LastUpdate == 0 && free == -1 )	free = i;
	}
	// If there was no match, we need to make one
	if(i == -1) {
		if(free != -1)
			i = free;
		else
			i = oldest;
		gaARP_Cache6[i].MAC = HWAddr;
	}
	
	gaARP_Cache6[i].IP = SWAddr;
	gaARP_Cache6[i].LastUpdate = now();
	giARP_LastUpdateID ++;
	Mutex_Release(&glARP_Cache6);
}
#endif

/**
 * \fn void ARP_int_GetPacket(tAdapter *Adapter, tMacAddr From, int Length, void *Buffer)
 * \brief Called when an ARP packet is recieved
 */
void ARP_int_GetPacket(tAdapter *Adapter, tMacAddr From, int Length, void *Buffer)
{
	tArpRequest4	*req4 = Buffer;
	#if ARPv6
	tArpRequest6	*req6 = Buffer;
	#endif
	tInterface	*iface;
	
	// Sanity Check Packet
	if( Length < (int)sizeof(tArpRequest4) ) {
		Log_Log("ARP", "Recieved undersized packet");
		return ;
	}
	if( ntohs(req4->Type) != 0x0800 ) {
		Log_Log("ARP", "Recieved a packet with a bad type (0x%x)", ntohs(req4->Type));
		return ;
	}
	if( req4->HWSize != 6 ) {
		Log_Log("ARP", "Recieved a packet with HWSize != 6 (%i)", req4->HWSize);
		return;
	}
	#if ARP_DETECT_SPOOFS
	if( !MAC_EQU(req4->SourceMac, From) ) {
		Log_Log("ARP", "ARP spoofing detected "
			"(%02x%02x:%02x%02x:%02x%02x != %02x%02x:%02x%02x:%02x%02x)",
			req4->SourceMac.B[0], req4->SourceMac.B[1], req4->SourceMac.B[2],
			req4->SourceMac.B[3], req4->SourceMac.B[4], req4->SourceMac.B[5],
			From.B[0], From.B[1], From.B[2],
			From.B[3], From.B[4], From.B[5]
			);
		return;
	}
	#endif
	
	switch( ntohs(req4->Request) )
	{
	case 1:	// You want my IP?
		// Check what type of IP it is
		switch( req4->SWSize )
		{
		case 4:
			Log_Debug("ARP", "ARP Request IPv4 Address %i.%i.%i.%i from %i.%i.%i.%i",
				req4->DestIP.B[0], req4->DestIP.B[1], req4->DestIP.B[2],
				req4->DestIP.B[3],
				req4->SourceIP.B[0], req4->SourceIP.B[1],
				req4->SourceIP.B[2], req4->SourceIP.B[3]);
			Log_Debug("ARP", " from MAC %02x:%02x:%02x:%02x:%02x:%02x",
				req4->SourceMac.B[0], req4->SourceMac.B[1],
				req4->SourceMac.B[2], req4->SourceMac.B[3],
				req4->SourceMac.B[4], req4->SourceMac.B[5]);
			iface = IPv4_GetInterface(Adapter, req4->DestIP, 0);
			if( iface )
			{
				ARP_UpdateCache4(req4->SourceIP, req4->SourceMac);
				
				req4->DestIP = req4->SourceIP;
				req4->DestMac = req4->SourceMac;
				req4->SourceIP = *(tIPv4*)iface->Address;;
				memcpy(&req4->SourceMac, Adapter->HWAddr, 6);	// TODO: Remove hard size
				req4->Request = htons(2);
				Log_Debug("ARP", "Sending back us (%02x:%02x:%02x:%02x:%02x:%02x)",
					req4->SourceMac.B[0], req4->SourceMac.B[1],
					req4->SourceMac.B[2], req4->SourceMac.B[3],
					req4->SourceMac.B[4], req4->SourceMac.B[5]);
				
				// Assumes only a header and footer at link layer
				tIPStackBuffer	*buffer = IPStack_Buffer_CreateBuffer(3);
				IPStack_Buffer_AppendSubBuffer(buffer, sizeof(struct sArpRequest4), 0, req4, NULL, NULL);
				Link_SendPacket(Adapter, 0x0806, req4->DestMac, buffer);
				IPStack_Buffer_DestroyBuffer(buffer);
			}
			break;
		#if ARPv6
		case 6:
			if( Length < (int)sizeof(tArpRequest6) ) {
				Log_Log("ARP", "Recieved undersized packet (IPv6)");
				return ;
			}
			Log_Debug("ARP", "ARP Request IPv6 Address %08x:%08x:%08x:%08x",
				ntohl(req6->DestIP.L[0]), ntohl(req6->DestIP.L[1]),
				ntohl(req6->DestIP.L[2]), ntohl(req6->DestIP.L[3])
				);
			iface = IPv6_GetInterface(Adapter, req6->DestIP, 0);
			if( iface )
			{
				req6->DestIP = req6->SourceIP;
				req6->DestMac = req6->SourceMac;
				req6->SourceIP = *(tIPv6*)iface->Address;
				req6->SourceMac = Adapter->MacAddr;
				req6->Request = htons(2);
				Log_Debug("ARP", "Sending back us (%02x:%02x:%02x:%02x:%02x:%02x)",
					req4->SourceMac.B[0], req4->SourceMac.B[1],
					req4->SourceMac.B[2], req4->SourceMac.B[3],
					req4->SourceMac.B[4], req4->SourceMac.B[5]);
				Link_SendPacket(Adapter, 0x0806, req6->DestMac, sizeof(tArpRequest6), req6);
			}
			break;
		#endif
		default:
			Log_Debug("ARP", "Unknown Protocol Address size (%i)", req4->SWSize);
			return ;
		}
		
		break;
	
	case 2:	// Ooh! A response!		
		// Check what type of IP it is
		switch( req4->SWSize )
		{
		case 4:
			ARP_UpdateCache4( req4->SourceIP, From );
			break;
		#if ARPv6
		case 6:
			if( Length < (int)sizeof(tArpRequest6) ) {
				Log_Debug("ARP", "Recieved undersized packet (IPv6)");
				return ;
			}
			ARP_UpdateCache6( req6->SourceIP, From );
			break;
		#endif
		default:
			Log_Debug("ARP", "Unknown Protocol Address size (%i)", req4->SWSize);
			return ;
		}
		
		break;
	
	default:
		Log_Warning("ARP", "Unknown Request ID %i", ntohs(req4->Request));
		break;
	}
}
