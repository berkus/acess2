/*
 * AxWin3 Interface Library
 * - By John Hodge (thePowersGang)
 *
 * msg.c
 * - Message handling / IPC
 */
#include <axwin3/axwin.h>
#include <acess/sys.h>
#include <string.h>
#include <stdlib.h>
#include <ipcmessages.h>	// AxWin3 common
#include "include/internal.h"
#include "include/ipc.h"

#define assert(cnd)	do{if(!(cnd)){_SysDebug("Assertion failed: %s", #cnd);}}while(0)

#define STATICBUF_SIZE	0x1000

// === CONSTANTS ===
enum eConnectionType
{
	CONNTYPE_NONE,
	CONNTYPE_SENDMESSAGE,
	CONNTYPE_IPCPIPE,
	CONNTYPE_UDP,
	CONNTYPE_TCP
};

// === GLOBALS ===
enum eConnectionType	giConnectionType;
int	giConnectionNum;	// FD or PID
char	gaAxWin3_int_UDPHeader[] = {5,16,0,0};	// Port 4101
 int	giAxWin3_int_UDPHeaderLen = sizeof(gaAxWin3_int_UDPHeader);
const char	*gsAxWin3_int_ServerDesc;
tAxWin3_MessageCallback	gAxWin3_MessageCallback;

// === CODE ===
void AxWin3_Connect(const char *ServerDesc)
{
	if( !ServerDesc ) {
		ServerDesc = gsAxWin3_int_ServerDesc;
	}
	_SysDebug("ServerDesc = %s", ServerDesc);
	if( !ServerDesc )
	{
		// TODO: Error out
		return ;
	}
	switch(ServerDesc[0])
	{
	case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case '0':
		giConnectionType = CONNTYPE_SENDMESSAGE;
		giConnectionNum = atoi(ServerDesc);
		if( giConnectionNum == 0 ) {
			_SysDebug("Invalid server PID");
			exit(-1);
		}
		break;
	case 'u':
		while(*ServerDesc && *ServerDesc != ':')	ServerDesc ++;
		ServerDesc ++;
		// TODO: Open socket and create UDP header
		break;
	case 't':
		while(*ServerDesc && *ServerDesc != ':')	ServerDesc ++;
		ServerDesc ++;
		// TODO: Open socket
		break;
	case 'p':
		assert( strncmp(ServerDesc, "pipe:", 5) == 0 );
		ServerDesc += 5;
		giConnectionType = CONNTYPE_IPCPIPE;
		giConnectionNum = _SysOpen(ServerDesc, OPENFLAG_READ|OPENFLAG_WRITE);
		if( giConnectionNum == -1 ) {
			_SysDebug("Cannot open IPC Pipe '%s'", ServerDesc);
			exit(-1);
		}
		break;
	default:
		_SysDebug("Unknown server desc format '%s'", ServerDesc);
		exit(-1);
		break;
	}
}

tAxWin3_MessageCallback AxWin3_SetMessageCallback(tAxWin3_MessageCallback Callback)
{
	tAxWin3_MessageCallback	old = gAxWin3_MessageCallback;
	gAxWin3_MessageCallback = Callback;
	return old;
}

uint32_t AxWin3_int_GetWindowID(tHWND Window)
{
	if(Window)
		return Window->ServerID;
	else
		return -1;
}

tAxWin_IPCMessage *AxWin3_int_AllocateIPCMessage(tHWND Window, int Message, int Flags, int ExtraBytes)
{
	tAxWin_IPCMessage	*ret;

	ret = malloc( sizeof(tAxWin_IPCMessage) + ExtraBytes );
	ret->Flags = Flags;
	ret->ID = Message;
	ret->Window = AxWin3_int_GetWindowID(Window);
	ret->Size = ExtraBytes;
	return ret;
}

void AxWin3_int_SendIPCMessage(tAxWin_IPCMessage *Msg)
{
	 int	size = sizeof(tAxWin_IPCMessage) + Msg->Size;
	switch(giConnectionType)
	{
	case CONNTYPE_SENDMESSAGE:
		_SysSendMessage(giConnectionNum, size, Msg);
		break;
	case CONNTYPE_UDP: {
		// Create UDP header
		char	tmpbuf[giAxWin3_int_UDPHeaderLen + size];
		memcpy(tmpbuf, gaAxWin3_int_UDPHeader, giAxWin3_int_UDPHeaderLen);
		memcpy(tmpbuf + giAxWin3_int_UDPHeaderLen, Msg, size);
		_SysWrite(giConnectionNum, tmpbuf, sizeof(tmpbuf));
		}
		break;
	case CONNTYPE_IPCPIPE:
	case CONNTYPE_TCP:
		_SysWrite(giConnectionNum, Msg, size);
		break;
	default:
		break;
	}
}

tAxWin_IPCMessage *AxWin3_int_GetIPCMessage(int nFD, fd_set *fds)
{
	 int	len;
	tAxWin_IPCMessage	*ret = NULL;
	 int	tid;
	fd_set	local_set;
	
	if(!fds)
		fds = &local_set;

	if( giConnectionType != CONNTYPE_SENDMESSAGE )
	{
		if(nFD <= giConnectionNum)
			nFD = giConnectionNum+1;
		FD_SET(giConnectionNum, fds);
	}

	_SysSelect(nFD, fds, NULL, NULL, NULL, THREAD_EVENT_IPCMSG);
	
	// Clear out IPC messages
	while( (len = _SysGetMessage(&tid, 0, NULL)) )
	{
		if( giConnectionType != CONNTYPE_SENDMESSAGE || tid != giConnectionNum )
		{
			_SysDebug("%i byte message from %i", len, tid);
			// If not, pass the buck (or ignore)
			if( gAxWin3_MessageCallback )
				gAxWin3_MessageCallback(tid, len);
			else
				_SysGetMessage(NULL, 0, GETMSG_IGNORE);
			continue ;
		}
		
		// Using CONNTYPE_SENDMESSAGE and server message has arrived
		ret = malloc(len);
		if(ret == NULL) {
			_SysDebug("malloc() failed, ignoring message");
			_SysGetMessage(NULL, 0, GETMSG_IGNORE);
			return NULL;
		}
		_SysGetMessage(NULL, len, ret);
		break;
	}

	if( giConnectionType != CONNTYPE_SENDMESSAGE )
	{
		if( FD_ISSET(giConnectionNum, fds) )
		{
			char	tmpbuf[STATICBUF_SIZE];
			char	*data = tmpbuf;
			size_t len = _SysRead(giConnectionNum, tmpbuf, sizeof(tmpbuf));
			
			if( giConnectionType == CONNTYPE_UDP )
			{
				assert(len > giAxWin3_int_UDPHeaderLen);
				len -= giAxWin3_int_UDPHeaderLen;
				data += giAxWin3_int_UDPHeaderLen;
			}
			assert(len >= sizeof(tAxWin_IPCMessage));
			ret = malloc(len);
			memcpy(ret, data, len);
		}
	}

	return ret;
}

tAxWin_IPCMessage *AxWin3_int_WaitIPCMessage(int WantedID)
{
	tAxWin_IPCMessage	*msg;
	for(;;)
	{
		msg = AxWin3_int_GetIPCMessage(0, NULL);
		if(msg->ID == WantedID)	return msg;
		AxWin3_int_HandleMessage( msg );
	}
}

