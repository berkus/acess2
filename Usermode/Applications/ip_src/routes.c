/*
 * Acess2 ip command
 * - By John Hodge (thePowersGang)
 * 
 * routes.c
 * - `ip route` sub-command
 */
#include "common.h"

#define DEFAULT_METRIC	30

// === PROTOTYPES ===
 int	Routes_main(int argc, char *argv[]);
void	DumpRoutes(void);
void	DumpRoute(int PFD, const char *Name);
void	AddRoute(const char *Interface, int AddressType, void *Dest, int MaskBits, int Metric, void *NextHop);

// === CODE ===
int Routes_main(int argc, char *argv[])
{
	if( argc <= 1 )
	{
		// Show routes
		DumpRoutes();
	}
	// Add new route
	else if( strcmp(argv[1], "add") == 0 )
	{
		uint8_t	dest[16] = {0};
		uint8_t	nextHop[16] = {0};
		 int	addrType, subnetBits = -1;
		 int	nextHopType, nextHopBits=-1;
		char	*ifaceName = NULL;
		 int	metric = DEFAULT_METRIC;
		// Usage:
		// ifconfig route add <host>[/<prefix>] <interface> [<metric>]
		// ifconfig route add <host>[/<prefix>] <next hop> [<metric>]
		if( argc - 2 < 2 ) {
			fprintf(stderr, "ERROR: '%s route add' takes at least two arguments, %i passed\n",
				argv[0], argc-3);
			PrintUsage(argv[0]);
			return -1;
		}
		
		if( argc - 2 > 3 ) {
			fprintf(stderr, "ERROR: '%s route add' takes at most three arguments, %i passed\n",
				argv[0], argc-3);
			PrintUsage(argv[0]);
			return -1;
		}
		
		// Destination IP
		addrType = ParseIPAddress(argv[2], dest, &subnetBits);
		if( subnetBits == -1 ) {
			subnetBits = Net_GetAddressSize(addrType)*8;
		}
		// Interface Name / Next Hop
		if( (nextHopType = ParseIPAddress(argv[3], nextHop, &nextHopBits)) == 0 )
		{
			// Interface name
			ifaceName = argv[3];
		}
		else
		{
			// Next Hop
			// - Check if it's the same type as the network/destination
			if( nextHopType != addrType ) {
				fprintf(stderr, "ERROR: Address type mismatch\n");
				return -1;
			}
			// - Make sure there's no mask
			if( nextHopBits != -1 ) {
				fprintf(stderr, "Error: Next hop cannot be masked\n");
				return -1;
			}
		}
		
		// Metric
		if( argc - 3 >= 3 )
		{
			metric = atoi(argv[4]);
			if( metric == 0 && argv[4][0] != '0' ) {
				fprintf(stderr, "ERROR: Metric should be a number\n");
				return -1;
			}
		}
		
		// Make the route!
		AddRoute(ifaceName, addrType, dest, subnetBits, metric, nextHop);
		
		return 0;
	}
	// Delete a route
	else if( strcmp(argv[2], "del") == 0 )
	{
		// Usage:
		// ifconfig route del <routenum>
		// ifconfig route del <host>[/<prefix>]
	}
	else
	{
		// List routes
		DumpRoutes();
	}
	
	return 0;
}

/**
 * \brief Dump all interfaces
 */
void DumpRoutes(void)
{
	 int	dp;
	char	filename[FILENAME_MAX+1];
	
	dp = _SysOpen(IPSTACK_ROOT"/routes", OPENFLAG_READ);
	
	printf("Type\tNetwork \tGateway \tMetric\tIFace\n");
	
	while( _SysReadDir(dp, filename) )
	{
		if(filename[0] == '.')	continue;
		DumpRoute(dp, filename);
	}
	
	_SysClose(dp);
}

/**
 * \brief Dump a route
 */
void DumpRoute(int PFD, const char *Name)
{
	 int	fd;
	 int	type;
	
	fd = _SysOpenChild(PFD, Name, OPENFLAG_READ);
	if(fd == -1) {
		printf("%s:\tUnable to open\n", Name);
		return ;
	}
	
	 int	ofs = 2;
	type = atoi(Name);
	
	 int	i;
	 int	len = Net_GetAddressSize(type);
	uint8_t	net[len], gw[len];
	 int	subnet, metric;
	for( i = 0; i < len; i ++ ) {
		char tmp[5] = "0x00";
		tmp[2] = Name[ofs++];
		tmp[3] = Name[ofs++];
		net[i] = atoi(tmp);
	}
	ofs ++;
	subnet = atoi(Name+ofs);
	ofs ++;
	metric = atoi(Name+ofs);
	_SysIOCtl(fd, _SysIOCtl(fd, 3, "get_nexthop"), gw);	// Get Gateway/NextHop
	
	// Get the address type
	switch(type)
	{
	case 0:	// Disabled/Unset
		printf("DISABLED\n");
		break;
	case 4:	// IPv4
		printf("IPv4\t");
		break;
	case 6:	// IPv6
		printf("IPv6\t");
		break;
	default:	// Unknow
		printf("UNKNOWN (%i)\n", type);
		break;
	}
	printf("%s/%i\t", Net_PrintAddress(type, net), subnet);
	printf("%s \t", Net_PrintAddress(type, gw));
	printf("%i\t", metric);
	
	// Interface
	{
		 int	call_num = _SysIOCtl(fd, 3, "get_interface");
		 int	len = _SysIOCtl(fd, call_num, NULL);
		char	*buf = malloc(len+1);
		_SysIOCtl(fd, call_num, buf);
		printf("'%s'\t", buf);
		free(buf);
	}
	
	printf("\n");
			
	_SysClose(fd);
}

void AddRoute(const char *Interface, int AddressType, void *Dest, int MaskBits, int Metric, void *NextHop)
{
	 int	fd;
	 int	num;
	char	*ifaceToFree = NULL;
	
	// Get interface name
	if( !Interface )
	{
		if( !NextHop ) {
			fprintf(stderr,
				"BUG: AddRoute(Interface=NULL,...,NextHop=NULL)\n"
				"Only one should be NULL\n"
				);
			return ;
		}
		
		// Query for the interface name
		Interface = ifaceToFree = Net_GetInterface(AddressType, NextHop);
	}
	// Check address type (if the interface was passed)
	// - If we got the interface name, then it should be correct
	else
	{
		char	ifacePath[sizeof(IPSTACK_ROOT"/")+strlen(Interface)+1];
		
		// Open interface
		strcpy(ifacePath, IPSTACK_ROOT"/");
		strcat(ifacePath, Interface);
		fd = _SysOpen(ifacePath, 0);
		if( fd == -1 ) {
			fprintf(stderr, "Error: Interface '%s' does not exist\n", Interface);
			return ;
		}
		// Get and check type
		num = _SysIOCtl(fd, _SysIOCtl(fd, 3, "getset_type"), NULL);
		if( num != AddressType ) {
			fprintf(stderr, "Error: Passed type does not match interface type (%i != %i)\n",
				AddressType, num);
			return ;
		}
		
		_SysClose(fd);
	}
	
	// Create route
	 int	addrsize = Net_GetAddressSize(AddressType);
	 int	len = snprintf(NULL, 0, "/Devices/ip/routes/%i::%i:%i", AddressType, MaskBits, Metric) + addrsize*2;
	char	path[len+1];
	{
		 int	i, ofs;
		ofs = sprintf(path, "/Devices/ip/routes/%i:", AddressType);
		for( i = 0; i < addrsize; i ++ )
			sprintf(path+ofs+i*2, "%02x", ((uint8_t*)Dest)[i]);
		ofs += addrsize*2;
		sprintf(path+ofs, ":%i:%i", MaskBits, Metric);
	}

	fd = _SysOpen(path, 0);
	if( fd != -1 ) {
		_SysClose(fd);
		fprintf(stderr, "Unable to create route '%s', already exists\n", path);
		return ;
	}
	fd = _SysOpen(path, OPENFLAG_CREATE, 0);
	if( fd == -1 ) {
		fprintf(stderr, "Unable to create '%s'\n", path);
		return ;
	}
	
	if( NextHop )
		_SysIOCtl(fd, _SysIOCtl(fd, 3, "set_nexthop"), NextHop);
	_SysIOCtl(fd, _SysIOCtl(fd, 3, "set_interface"), (void*)Interface);
	
	_SysClose(fd);
	
	// Check if the interface name was allocated by us
	if( ifaceToFree )
		free(ifaceToFree);
}

