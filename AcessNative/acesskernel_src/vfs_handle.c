/*
 * Acess2 VFS
 * - AllocHandle, GetHandle
 */
#define DEBUG	0
#include <acess.h>
#include <vfs.h>
#include <vfs_int.h>
#include <vfs_ext.h>
#include <threads.h>

// === CONSTANTS ===
#define MAX_KERNEL_FILES	128
#define MAX_USER_FILES	64

// === IMPORTS ===
extern int	Server_GetClientID(void);

// === PROTOTYPES ===
tVFS_Handle	*VFS_GetHandle(int FD);
 int	VFS_AllocHandle(int FD, tVFS_Node *Node, int Mode);

// === Types ===
typedef struct sUserHandles
{
	struct sUserHandles	*Next;
	 int	PID;
	tVFS_Handle	Handles[MAX_USER_FILES];
}	tUserHandles;

// === GLOBALS ===
tUserHandles	*gpUserHandles = NULL;
tVFS_Handle	gaKernelHandles[MAX_KERNEL_FILES];

// === CODE ===
tUserHandles *VFS_int_GetUserHandles(int PID, int bCreate)
{
	tUserHandles	*ent, *prev = NULL;
	for( ent = gpUserHandles; ent; prev = ent, ent = ent->Next ) {
		if( ent->PID == PID ) {
			//if( bCreate )
			//	Log_Warning("VFS", "Process %i already has a handle list", PID);
			return ent;
		}
		if( ent->PID > PID )	break;
	}
	
	if(!bCreate)
		return NULL;
	
	ent = calloc( 1, sizeof(tUserHandles) );
	ent->PID = PID;
	if( prev ) {
		ent->Next = prev->Next;
		prev->Next = ent;
	}
	else {
		ent->Next = gpUserHandles;
		gpUserHandles = ent;
	}
	Log_Notice("VFS", "Created handle list for process %i", PID);
	return ent;
}

/**
 * \brief Clone the handle list of the current process into another
 */
void VFS_CloneHandleList(int PID)
{
	tUserHandles	*ent;
	tUserHandles	*cur;
	 int	i, maxhandles;
	
	cur = VFS_int_GetUserHandles(Threads_GetPID(), 0);
	if(!cur)	return ;	// Don't need to do anything if the current list is empty
	
	ent = VFS_int_GetUserHandles(PID, 1);
	
	maxhandles = *Threads_GetMaxFD();
	memcpy(ent->Handles, cur->Handles, maxhandles*sizeof(tVFS_Handle));
	
	for( i = 0; i < maxhandles; i ++ )
	{
		if(!cur->Handles[i].Node)	continue;
		
		if(ent->Handles[i].Node->Type->Reference)
			ent->Handles[i].Node->Type->Reference(ent->Handles[i].Node);
	}
}

void VFS_CloneHandlesFromList(int PID, int nFD, int FDs[])
{
	tUserHandles	*ent;
	tUserHandles	*cur;
	 int	i, maxhandles;
	
	cur = VFS_int_GetUserHandles(Threads_GetPID(), 0);
	if(!cur)	return ;	// Don't need to do anything if the current list is empty
	
	ent = VFS_int_GetUserHandles(PID, 1);
	
	maxhandles = *Threads_GetMaxFD();
	if( nFD > maxhandles )
		nFD = maxhandles;
	for( i = 0; i < nFD; i ++ )
	{
		if( FDs[i] >= maxhandles ) {
			ent->Handles[i].Node = NULL;
			continue ;
		}
		memcpy(&ent->Handles[i], &cur->Handles[ FDs[i] ], sizeof(tVFS_Handle));
	}
	for( ; i < maxhandles; i ++ )
		cur->Handles[i].Node = NULL;
	
	for( i = 0; i < maxhandles; i ++ )
	{
		if(!cur->Handles[i].Node)	continue;
		
		if(ent->Handles[i].Node->Type->Reference)
			ent->Handles[i].Node->Type->Reference(ent->Handles[i].Node);
	}
}

/**
 * \fn tVFS_Handle *VFS_GetHandle(int FD)
 * \brief Gets a pointer to the handle information structure
 */
tVFS_Handle *VFS_GetHandle(int FD)
{
	tVFS_Handle	*h;
	
	//Log_Debug("VFS", "VFS_GetHandle: (FD=0x%x)", FD);
	
	if(FD < 0) {
		LOG("FD (%i) < 0, RETURN NULL", FD);
		return NULL;
	}
	
	if(FD & VFS_KERNEL_FLAG) {
		FD &= (VFS_KERNEL_FLAG - 1);
		if(FD >= MAX_KERNEL_FILES) {
			LOG("FD (%i) > MAX_KERNEL_FILES (%i), RETURN NULL", FD, MAX_KERNEL_FILES);
			return NULL;
		}
		h = &gaKernelHandles[ FD ];
	}
	else
	{
		tUserHandles	*ent;
		 int	pid = Threads_GetPID();
		 int	maxhandles = *Threads_GetMaxFD();
		
		ent = VFS_int_GetUserHandles(pid, 0);
		if(!ent) {
			Log_Error("VFS", "Client %i does not have a handle list (>)", pid);
			return NULL;
		}
		
		if(FD >= maxhandles) {
			LOG("FD (%i) > Limit (%i), RETURN NULL", FD, maxhandles);
			return NULL;
		}
		h = &ent->Handles[ FD ];
		LOG("FD (%i) -> %p (Mode:0x%x,Node:%p)", FD, h, h->Mode, h->Node);
	}
	
	if(h->Node == NULL) {
		LOG("FD (%i) Unused", FD);
		return NULL;
	}
	//Log_Debug("VFS", "VFS_GetHandle: RETURN %p", h);
	return h;
}

int VFS_SetHandle(int FD, tVFS_Node *Node, int Mode)
{
	tVFS_Handle	*h;
	if(FD < 0)	return -1;
	
	if( FD & VFS_KERNEL_FLAG ) {
		FD &= (VFS_KERNEL_FLAG -1);
		if( FD >= MAX_KERNEL_FILES )	return -1;
		h = &gaKernelHandles[FD];
	}
	else {
		tUserHandles	*ent;
		 int	pid = Threads_GetPID();
		 int	maxhandles = *Threads_GetMaxFD();
		
		ent = VFS_int_GetUserHandles(pid, 0);
		if(!ent) {
			Log_Error("VFS", "Client %i does not have a handle list (>)", pid);
			return NULL;
		}
		
		if(FD >= maxhandles) {
			LOG("FD (%i) > Limit (%i), RETURN NULL", FD, maxhandles);
			return NULL;
		}
		h = &ent->Handles[ FD ];
	}
	h->Node = Node;
	h->Mode = Mode;
	return FD;
}


int VFS_AllocHandle(int bIsUser, tVFS_Node *Node, int Mode)
{
	 int	i;
	
	// Check for a user open
	if(bIsUser)
	{
		tUserHandles	*ent;
		 int	maxhandles = *Threads_GetMaxFD();
		// Find the PID's handle list
		ent = VFS_int_GetUserHandles(Threads_GetPID(), 1);
		// Get a handle
		for( i = 0; i < maxhandles; i ++ )
		{
			if(ent->Handles[i].Node)	continue;
			ent->Handles[i].Node = Node;
			ent->Handles[i].Position = 0;
			ent->Handles[i].Mode = Mode;
			return i;
		}
	}
	else
	{
		// Get a handle
		for(i=0;i<MAX_KERNEL_FILES;i++)
		{
			if(gaKernelHandles[i].Node)	continue;
			gaKernelHandles[i].Node = Node;
			gaKernelHandles[i].Position = 0;
			gaKernelHandles[i].Mode = Mode;
			return i|VFS_KERNEL_FLAG;
		}
	}
	
	return -1;
}
