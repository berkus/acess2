/*
 AcessOS Test Network Application
*/
#include <acess/sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <net.h>
#include <readline.h>
#include <string.h>

// === PROTOTYPES ===
 int	main(int argc, char *argv[], char *envp[]);
 int	OpenTCP(const char *AddressString, short PortNumber);

// ==== CODE ====
int main(int argc, char *argv[], char *envp[])
{
	 int	server_fd, rv;
	 int	client_running = 1;
	 int	bUseReadline = !!argv[3];	// HACK: If third argument is present, use ReadLine
	tReadline	*readline_info;
	 int	port;
	
	if( argc < 2 || argc > 3 ) {
		fprintf(stderr, "Usage: telnet <host> [<port>]\n Port defaults to 23\n");
		return 0;
	}
	
	if(argc == 3)
		port = atoi(argv[2]);
	else
		port = 23;
	
	// Connect to the remove server
	server_fd = OpenTCP( argv[1], port );
	if( server_fd == -1 ) {
		fprintf(stderr, "Unable to create socket\n");
		return -1;
	}
	
	// Create a ReadLine instance with no history
	readline_info = Readline_Init(0);
	
	while( client_running )
	{
		fd_set	fds;
		fd_set	err_fds;
		char	buffer[BUFSIZ];
		 int	len;
		
		// Prepare FD sets
		FD_ZERO(&fds);	FD_ZERO(&err_fds);
		FD_SET(0, &fds);	FD_SET(0, &err_fds);
		FD_SET(server_fd, &fds);	FD_SET(server_fd, &err_fds);
		
		// Wait for data (no timeout)
		rv = select(server_fd+1, &fds, NULL, &err_fds, NULL);
		if( rv < 0 )	break;
		
		// Check for remote data avaliable
		if( FD_ISSET(server_fd, &fds) )
		{
			// Read from server, and write to stdout
			do
			{
				len = _SysRead(server_fd, buffer, BUFSIZ);
				_SysWrite(1, buffer, len);
			} while( len == BUFSIZ );
		}
		
		// Check for local data input
		if( FD_ISSET(0, &fds) )
		{
			// Read from stdin, and write to server
			if( bUseReadline )
			{
				char	*line = Readline_NonBlock(readline_info);
				if( line )
				{
					_SysWrite(server_fd, line, strlen(line));
					_SysWrite(server_fd, "\n", 1);
				}
			}
			else
			{
				do
				{
					len = _SysRead(0, buffer, BUFSIZ);
					_SysWrite(server_fd, buffer, len);
					_SysWrite(1, buffer, len);
				} while( len == BUFSIZ );
			}
		}
		
		// If there was an error, quit
		if( FD_ISSET(server_fd, &err_fds) )
		{
			printf("\nRemote connection lost\n");
			break ;
		}
	}
	
	_SysClose(server_fd);
	return 0;
}

/**
 * \brief Initialise a TCP connection to \a AddressString on port \a PortNumber
 */
int OpenTCP(const char *AddressString, short PortNumber)
{
	 int	fd, addrType;
	uint8_t	addrBuffer[16];
	
	// Parse IP Address
	addrType = Net_ParseAddress(AddressString, addrBuffer);
	if( addrType == 0 ) {
		fprintf(stderr, "Unable to parse '%s' as an IP address\n", AddressString);
		return -1;
	}

	// Opens a R/W handle
	fd = Net_OpenSocket(addrType, addrBuffer, "tcpc");
	if( fd == -1 )
	{
		fprintf(stderr, "Unable to open TCP Client\n");
		return -1;
	}
	
	// Set remote port and address
	_SysIOCtl(fd, 5, &PortNumber);
	_SysIOCtl(fd, 6, addrBuffer);
	
	// Connect
	if( _SysIOCtl(fd, 7, NULL) == 0 ) {
		fprintf(stderr, "Unable to start connection\n");
		return -1;
	}
	
	printf("Connection opened\n");
	
	// Return descriptor
	return fd;
}

