/* 
 * Acess Micro VFS
 */
#include <acess.h>
#include "vfs.h"
#include "vfs_int.h"

// === GLOBALS ===
tVFS_ACL	gVFS_ACL_EveryoneRWX = { {1,-1}, {0,VFS_PERM_ALL} };
tVFS_ACL	gVFS_ACL_EveryoneRW = { {1,-1}, {0,VFS_PERM_ALL^VFS_PERM_EXECUTE} };
tVFS_ACL	gVFS_ACL_EveryoneRX = { {1,-1}, {0,VFS_PERM_READ|VFS_PERM_EXECUTE} };
tVFS_ACL	gVFS_ACL_EveryoneRO = { {1,-1}, {0,VFS_PERM_READ} };

// === CODE ===
/**
 * \fn int VFS_CheckACL(tVFS_Node *Node, Uint Permissions)
 * \brief Checks the permissions on a file
 */
int VFS_CheckACL(tVFS_Node *Node, Uint Permissions)
{
	 int	i;
	 int	uid = Threads_GetUID();
	 int	gid = Threads_GetGID();
	
	// Root can do anything
	if(uid == 0)	return 1;
	
	// Root only file?, fast return
	if( Node->NumACLs == 0 ) {
		Log("VFS_CheckACL - %p inaccesable, NumACLs = 0, uid=%i", Node, uid);
		return 0;
	}
	
	// Check Deny Permissions
	for(i=0;i<Node->NumACLs;i++)
	{
		if(!Node->ACLs[i].Inv)	continue;	// Ignore ALLOWs
		if(Node->ACLs[i].ID != 0x7FFFFFFF)
		{
			if(!Node->ACLs[i].Group && Node->ACLs[i].ID != uid)	continue;
			if(Node->ACLs[i].Group && Node->ACLs[i].ID != gid)	continue;
		}
		
		//Log("Deny %x", Node->ACLs[i].Perms);
		
		if(Node->ACLs[i].Perms & Permissions) {
			Log("VFS_CheckACL - %p inaccesable, %x denied",
				Node, Node->ACLs[i].Perms & Permissions);
			return 0;
		}
	}
	
	// Check for allow permissions
	for(i=0;i<Node->NumACLs;i++)
	{
		if(Node->ACLs[i].Inv)	continue;	// Ignore DENYs
		if(Node->ACLs[i].ID != 0x7FFFFFFF)
		{
			if(!Node->ACLs[i].Group && Node->ACLs[i].ID != uid)	continue;
			if(Node->ACLs[i].Group && Node->ACLs[i].ID != gid)	continue;
		}
		
		//Log("Allow %x", Node->ACLs[i].Perms);
		
		if((Node->ACLs[i].Perms & Permissions) == Permissions)	return 1;
	}
	
	Log("VFS_CheckACL - %p inaccesable, %x not allowed", Node, Permissions);
	return 0;
}
/**
 * \fn int VFS_GetACL(int FD, tVFS_ACL *Dest)
 */
int VFS_GetACL(int FD, tVFS_ACL *Dest)
{
	 int	i;
	tVFS_Handle	*h = VFS_GetHandle(FD);
	
	// Error check
	if(!h) {
		return -1;
	}
	
	// Root can do anything
	if(Dest->Group == 0 && Dest->ID == 0) {
		Dest->Inv = 0;
		Dest->Perms = -1;
		return 1;
	}
	
	// Root only file?, fast return
	if( h->Node->NumACLs == 0 ) {
		Dest->Inv = 0;
		Dest->Perms = 0;
		return 0;
	}
	
	// Check Deny Permissions
	for(i=0;i<h->Node->NumACLs;i++)
	{
		if(h->Node->ACLs[i].Group != Dest->Group)	continue;
		if(h->Node->ACLs[i].ID != Dest->ID)	continue;
		
		Dest->Inv = h->Node->ACLs[i].Inv;
		Dest->Perms = h->Node->ACLs[i].Perms;
		return 1;
	}
	
	
	Dest->Inv = 0;
	Dest->Perms = 0;
	return 0;
}

/**
 * \fn tVFS_ACL *VFS_UnixToAcessACL(Uint Mode, Uint Owner, Uint Group)
 * \brief Converts UNIX permissions to three Acess ACL entries
 */
tVFS_ACL *VFS_UnixToAcessACL(Uint Mode, Uint Owner, Uint Group)
{
	tVFS_ACL	*ret = malloc(sizeof(tVFS_ACL)*3);
	
	// Error Check
	if(!ret)	return NULL;
	
	// Owner
	ret[0].Group = 0;	ret[0].ID = Owner;
	ret[0].Inv = 0;		ret[0].Perms = 0;
	if(Mode & 0400)	ret[0].Perms |= VFS_PERM_READ;
	if(Mode & 0200)	ret[0].Perms |= VFS_PERM_WRITE;
	if(Mode & 0100)	ret[0].Perms |= VFS_PERM_EXECUTE;
	
	// Group
	ret[1].Group = 1;	ret[1].ID = Group;
	ret[1].Inv = 0;		ret[1].Perms = 0;
	if(Mode & 0040)	ret[1].Perms |= VFS_PERM_READ;
	if(Mode & 0020)	ret[1].Perms |= VFS_PERM_WRITE;
	if(Mode & 0010)	ret[1].Perms |= VFS_PERM_EXECUTE;
	
	// Global
	ret[2].Group = 1;	ret[2].ID = -1;
	ret[2].Inv = 0;		ret[2].Perms = 0;
	if(Mode & 0004)	ret[2].Perms |= VFS_PERM_READ;
	if(Mode & 0002)	ret[2].Perms |= VFS_PERM_WRITE;
	if(Mode & 0001)	ret[2].Perms |= VFS_PERM_EXECUTE;
	
	// Return buffer
	return ret;
}

// === EXPORTS ===
// --- Variables ---
EXPORTV(gVFS_ACL_EveryoneRWX);
EXPORTV(gVFS_ACL_EveryoneRW);
EXPORTV(gVFS_ACL_EveryoneRX);
// --- Functions ---
EXPORT(VFS_UnixToAcessACL);