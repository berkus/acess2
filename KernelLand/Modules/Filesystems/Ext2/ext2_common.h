/*
 * Acess OS
 * Ext2 Driver Version 1
 */
#ifndef _EXT2_COMMON_H
#define _EXT2_COMMON_H
#include <acess.h>
#include <vfs.h>
#include "ext2fs.h"

#define EXT2_UPDATE_WRITEBACK	1

// === STRUCTURES ===
typedef struct {
	 int	FD;
	tInodeCache	*CacheID;
	tVFS_Node	RootNode;
	
	tExt2_SuperBlock	SuperBlock;
	Uint	BlockSize;
	 
	 int	GroupCount;
	tExt2_Group		Groups[];
} tExt2_Disk;

// === GLOBALS ===
extern tVFS_NodeType	gExt2_FileType;
extern tVFS_NodeType	gExt2_DirType;

// === FUNCTIONS ===
// --- Common ---
extern void	Ext2_CloseFile(tVFS_Node *Node);
extern Uint64	Ext2_int_GetBlockAddr(tExt2_Disk *Disk, Uint32 *Blocks, int BlockNum);
extern void	Ext2_int_UpdateSuperblock(tExt2_Disk *Disk);
extern Uint32	Ext2_int_AllocateInode(tExt2_Disk *Disk, Uint32 Parent);
extern void	Ext2_int_DereferenceInode(tExt2_Disk *Disk, Uint32 Inode);
extern int	Ext2_int_ReadInode(tExt2_Disk *Disk, Uint32 InodeId, tExt2_Inode *Inode);
extern int	Ext2_int_WriteInode(tExt2_Disk *Disk, Uint32 InodeId, tExt2_Inode *Inode);
// --- Dir ---
extern int	Ext2_ReadDir(tVFS_Node *Node, int Pos, char Dest[FILENAME_MAX]);
extern tVFS_Node	*Ext2_FindDir(tVFS_Node *Node, const char *FileName, Uint Flags);
extern tVFS_Node	*Ext2_MkNod(tVFS_Node *Node, const char *Name, Uint Mode);
extern int	Ext2_Link(tVFS_Node *Parent, const char *Name, tVFS_Node *Node);
extern tVFS_Node	*Ext2_int_CreateNode(tExt2_Disk *Disk, Uint InodeId);
extern int	Ext2_int_WritebackNode(tExt2_Disk *Disk, tVFS_Node *Node);
// --- Read ---
extern size_t	Ext2_Read(tVFS_Node *node, off_t offset, size_t length, void *buffer, Uint Flags);
// --- Write ---
extern size_t	Ext2_Write(tVFS_Node *node, off_t offset, size_t length, const void *buffer, Uint Flags);
extern Uint32	Ext2_int_AllocateBlock(tExt2_Disk *Disk, Uint32 LastBlock);
extern void	Ext2_int_DeallocateBlock(tExt2_Disk *Disk, Uint32 Block);
extern int	Ext2_int_AppendBlock(tExt2_Disk *Disk, tExt2_Inode *Inode, Uint32 Block);

#endif
