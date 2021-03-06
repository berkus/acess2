/*
 * Acess OS
 * InitRD Driver Version 1
 */
#include "initrd.h"
#include <modules.h>

#define DUMP_ON_MOUNT	0

// === IMPORTS ==
extern tVFS_Node	gInitRD_RootNode;
extern const int	giInitRD_NumFiles;
extern tVFS_Node * const	gInitRD_FileList[];

// === PROTOTYPES ===
 int	InitRD_Install(char **Arguments);
tVFS_Node	*InitRD_InitDevice(const char *Device, const char **Arguments);
void	InitRD_Unmount(tVFS_Node *Node);
tVFS_Node	*InitRD_GetNodeFromINode(tVFS_Node *Root, Uint64 Inode);
size_t	InitRD_ReadFile(tVFS_Node *Node, off_t Offset, size_t Size, void *Buffer, Uint Flags);
 int	InitRD_ReadDir(tVFS_Node *Node, int ID, char Dest[FILENAME_MAX]);
tVFS_Node	*InitRD_FindDir(tVFS_Node *Node, const char *Name, Uint Flags);
void	InitRD_DumpDir(tVFS_Node *Node, int Indent);

// === GLOBALS ===
MODULE_DEFINE(0, 0x0A, FS_InitRD, InitRD_Install, NULL);
tVFS_Driver	gInitRD_FSInfo = {
	.Name = "initrd",
	.InitDevice = InitRD_InitDevice,
	.Unmount = InitRD_Unmount,
	.GetNodeFromINode = InitRD_GetNodeFromINode
	};
tVFS_NodeType	gInitRD_DirType = {
	.ReadDir = InitRD_ReadDir,
	.FindDir = InitRD_FindDir
	};
tVFS_NodeType	gInitRD_FileType = {
	.Read = InitRD_ReadFile
	};

/**
 * \brief Register initrd with the kernel
 */
int InitRD_Install(char **Arguments)
{
	VFS_AddDriver( &gInitRD_FSInfo );
	
	return MODULE_ERR_OK;
}

/**
 * \brief Mount the InitRD
 */
tVFS_Node *InitRD_InitDevice(const char *Device, const char **Arguments)
{
	#if DUMP_ON_MOUNT
	InitRD_DumpDir( &gInitRD_RootNode, 0 );
	#endif
	return &gInitRD_RootNode;
}

/**
 * \brief Unmount the InitRD
 */
void InitRD_Unmount(tVFS_Node *Node)
{
}

/**
 */
tVFS_Node *InitRD_GetNodeFromINode(tVFS_Node *Root, Uint64 Inode)
{
	if( Inode >= giInitRD_NumFiles )	return NULL;
	return gInitRD_FileList[Inode];
}

/**
 * \brief Read from a file
 */
size_t InitRD_ReadFile(tVFS_Node *Node, off_t Offset, size_t Length, void *Buffer, Uint Flags)
{
	if(Offset > Node->Size)
		return 0;
	if(Offset + Length > Node->Size)
		Length = Node->Size - Offset;
	
	memcpy(Buffer, Node->ImplPtr+Offset, Length);
	
	return Length;
}

/**
 * \brief Read from a directory
 */
int InitRD_ReadDir(tVFS_Node *Node, int ID, char Dest[FILENAME_MAX])
{
	tInitRD_File	*dir = Node->ImplPtr;
	
	if(ID >= Node->Size)
		return -EINVAL;
	
	strncpy(Dest, dir[ID].Name, FILENAME_MAX);
	return 0;
}

/**
 * \brief Find an element in a directory
 */
tVFS_Node *InitRD_FindDir(tVFS_Node *Node, const char *Name, Uint Flags)
{
	 int	i;
	tInitRD_File	*dir = Node->ImplPtr;
	
	LOG("Name = '%s'", Name);
	
	for( i = 0; i < Node->Size; i++ )
	{
		if(strcmp(Name, dir[i].Name) == 0)
			return dir[i].Node;
	}
	
	return NULL;
}

void InitRD_DumpDir(tVFS_Node *Node, int Indent)
{
	 int	i;
	char	indent[Indent+1];
	tInitRD_File	*dir = Node->ImplPtr;
	
	for( i = 0; i < Indent;	i++ )	indent[i] = ' ';
	indent[i] = '\0';
	
	for( i = 0; i < Node->Size; i++ )
	{
		Log_Debug("InitRD", "%s- %p %s", indent, dir[i].Node, dir[i].Name);
		if(dir[i].Node->Flags & VFS_FFLAG_DIRECTORY)
			InitRD_DumpDir(dir[i].Node, Indent+1);
	}
}
