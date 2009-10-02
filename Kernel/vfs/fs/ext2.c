/*
 * Acess OS
 * Ext2 Driver Version 1
 */
/**
 * \file fs/ext2.c
 * \brief Second Extended Filesystem Driver
 * \todo Implement file read support
 */
#include <common.h>
#include <vfs.h>
#include "fs_ext2.h"

// === STRUCTURES ===
typedef struct {
	 int	FD;
	 int	CacheID;
	vfs_node	RootNode;
	
	tExt2_SuperBlock	SuperBlock;
	 int	BlockSize;
	 
	 int	GroupCount;
	tExt2_Group		Groups[];
} tExt2_Disk;

// === PROTOTYPES ===
//Interface Functions
tVFS_Node	*Ext2_InitDevice(char *Device, char **Options);
void		Ext2_UnMount(tVFS_Node *Node);
Uint64		Ext2_Read(tVFS_Node *node, Uint64 offset, Uint64 length, void *buffer);
Uint64		Ext2_Write(tVFS_Node *node, Uint64 offset, Uint64 length, void *buffer);
char		*Ext2_ReadDir(tVFS_Node *Node, int Pos);
tVFS_Node	*Ext2_FindDir(tVFS_Node *Node, char *FileName);
tVFS_Node	*Ext2_MkNod(tVFS_Node *Node, char *Name, Uint Flags);
 int		Ext2_int_GetInode(vfs_node *Node, tExt2_Inode *Inode);
tVFS_Node	*Ext2_int_CreateNode(tExt2_Disk *Disk, Uint InodeId, char *Name, Uint64 VfsInode);
 int		Ext2_int_ReadInode(tExt2_Disk *Disk, Uint InodeId, tExt2_Inode *Inode);
Uint64		Ext2_int_GetBlockAddr(tExt2_Disk *Disk, Uint32 *Blocks, int BlockNum);

// === SEMI-GLOBALS ===
tExt2_Disk	gExt2_disks[6];
 int	giExt2_count = 0;
tVFS_Driver	gExt2_FSInfo = {NULL,
	"ext2", 0, Ext2_InitDevice, Ext2_UnMount, NULL
	};

// === CODE ===

/**
 * \fn void Ext2_Install()
 * \brief Install the Ext2 Filesystem Driver
 */
void Ext2_Install()
{
	VFS_AddDriver( &gExt2_FSInfo );
}

/**
 \fn tVFS_Node *Ext2_initDevice(char *Device, char **Options)
 \brief Initializes a device to be read by by the driver
 \param Device	String - Device to read from
 \param Options	NULL Terminated array of option strings
 \return Root Node
*/
tVFS_Node *Ext2_InitDevice(char *Device, char **Options)
{
	tExt2_Disk	*disk;
	 int	fd;
	 int	groupCount;
	tExt2_SuperBlock	sb;
	tExt2_Inode	inode;
	
	// Open Disk
	fd = VFS_Open(Device, VFS_OPENFLAG_READ|VFS_OPENFLAG_WRITE);		//Open Device
	if(fd == -1) {
		Warning"[EXT2] Unable to open '%s'\n", Device);
		return NULL;
	}
	
	// Read Superblock at offset 1024
	VFS_ReadAt(fd, 1024, 1024, &sb);	// Read Superblock
	
	// Sanity Check Magic value
	if(sb.s_magic != 0xEF53) {
		WarningEx("EXT2", "Volume '%s' is not an EXT2 volume\n", Device);
		VFS_Close(fd);
		return NULL;
	}
	
	// Get Group count
	groupCount = DivUp(sb.s_blocks_count, sb.s_blocks_per_group);
	//LogF(" Ext2_initDevice: groupCount = %i\n", groupCount);
	
	// Allocate Disk Information
	disk = malloc(sizeof(tExt2_Disk) + sizeof(tExt2_Group)*groupCount);
	disk->fd = fd;
	memcpy(&disk->SuperBlock, &sb, 1024);
	disk->GroupCount = groupCount;
	
	// Get an inode cache handle
	disk->CacheID = Inode_GetHandle();
	
	// Get Block Size
	//LogF(" Ext2_initDevice: s_log_block_size = 0x%x\n", sb.s_log_block_size);
	disk->BlockSize = 1024 << sb.s_log_block_size;
	
	// Read Group Information
	VFS_ReadAt(disk->fd,
		sb.s_first_data_block * disk->BlockSize + 1024,
		sizeof(tExt2_Group)*groupCount,
		disk->Groups);
	
	#if 0
	Log(" Ext2_initDevice: Block Group 0\n");
	Log(" Ext2_initDevice: .bg_block_bitmap = 0x%x\n", disk->Groups[0].bg_block_bitmap);
	Log(" Ext2_initDevice: .bg_inode_bitmap = 0x%x\n", disk->Groups[0].bg_inode_bitmap);
	Log(" Ext2_initDevice: .bg_inode_table = 0x%x\n", disk->Groups[0].bg_inode_table);
	Log(" Ext2_initDevice: Block Group 1\n");
	Log(" Ext2_initDevice: .bg_block_bitmap = 0x%x\n", disk->Groups[1].bg_block_bitmap);
	Log(" Ext2_initDevice: .bg_inode_bitmap = 0x%x\n", disk->Groups[1].bg_inode_bitmap);
	Log(" Ext2_initDevice: .bg_inode_table = 0x%x\n", disk->Groups[1].bg_inode_table);
	#endif
	
	// Get root Inode
	Ext2_int_ReadInode(disk, 2, &inode);
	
	// Create Root Node
	memset(&disk->RootNode, 0, sizeof(vfs_node));
	disk->RootNode.Inode = 2;	// Root inode ID
	disk->RootNode.ImplPtr = disk;	// Save disk pointer
	disk->RootNode.Size = -1;	// Fill in later (on readdir)
	disk->RootNode.Flags = VFS_FFLAG_DIRECTORY;
	
	disk->RootNode.ReadDir = Ext2_ReadDir;
	disk->RootNode.FindDir = Ext2_FindDir;
	//disk->RootNode.Relink = Ext2_Relink;
	
	// Complete root node
	disk->RootNode.UID = inode.i_uid;
	disk->RootNode.GID = inode.i_gid;
	disk->RootNode.NumACLs = 1;
	disk->RootNode.ACLs = &gVFS_ACL_EveryoneRW;
	
	#if 0
	Log(" Ext2_InitDevice: inode.i_size = 0x%x\n", inode.i_size);
	Log(" Ext2_InitDevice: inode.i_block[0] = 0x%x\n", inode.i_block[0]);
	#endif
	
	return &disk->RootNode;
}

/**
 * \fn void Ext2_Unmount(tVFS_Node *Node)
 * \brief Close a mounted device
 */
void Ext2_Unmount(tVFS_Node *Node)
{
	tExt2_Disk	*disk = Node->ImplPtr;
	
	VFS_Close( disk->fd );
	Inode_ClearCache( disk->CacheID );
	memset(disk, 0, sizeof(tExt2_Disk)+disk->GroupCount*sizeof(tExt2_Group));
	free(disk);
}

/**
 * \fn Uint64 Ext2_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
 * \brief Read from a file
 */
Uint64 Ext2_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	tExt2_Disk	*disk = Node->ImplPtr;
	tExt2_Inode	inode;
	Uint64	base;
	Uint	block;
	Uint64	remLen;
	
	// Get Inode
	Ext2_int_GetInode(Node, &inode);
	
	block = Offset / disk->BlockSize;
	Offset = Offset / disk->BlockSize;
	base = Ext2_int_GetBlockAddr(disk, inode.i_block, block);
	
	// Read only block
	if(Length <= disk->BlockSize - Offset)
	{
		VFS_ReadAt( disk->fd, base+Offset, Length, Buffer);
		return Length;
	}
	
	// Read first block
	remLen = Length;
	VFS_ReadAt( disk->fd, base + Offset, disk->BlockSize - Offset, Buffer);
	remLen -= disk->BlockSize - Offset;
	Buffer += disk->BlockSize - Offset;
	block ++;
	
	// Read middle blocks
	while(remLen > disk->BlockSize)
	{
		base = Ext2_int_GetBlockAddr(disk, inode.i_block, block);
		VFS_ReadAt( disk->fd, base, disk->BlockSize, Buffer);
		Buffer += disk->BlockSize;
		remLen -= disk->BlockSize;
		block ++;
	}
	
	// Read last block
	base = Ext2_int_GetBlockAddr(disk, inode.i_block, block);
	VFS_ReadAt( disk->fd, base, remLen, Buffer);
	
	return Length;
}

/**
 * \fn Uint64 Ext2_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
 * \brief Write to a file
 */
Uint64 Ext2_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	tExt2_Disk	*disk = Node->ImplPtr;
	tExt2_Inode	inode;
	Uint64	base;
	Uint64	retLen;
	Uint	block;
	Uint64	allocSize;
	 int	bNewBlocks = 0;
	
	Ext2_int_GetInode(Node, &inode);
	
	// Round size up to block size
	// block size is a power of two, so this will work
	allocSize = (inode.i_size + disk->BlockSize) & ~(disk->BlockSize-1);
	if( Offset < allocSize )
	{
		if(Offset + Length > allocSize) {
			bNewBlocks = 1;
			retLen = allocSize - Offset;
		} else
			retLen = Length;
		// Within the allocated space
		block = Offset / disk->BlockSize;
		Offset %= disk->BlockSize;
		base = Ext2_int_GetBlockAddr(disk, inode.i_block, block);
		
		// Write only block (if only one)
		if(Offset + retLen <= disk->BlockSize) {
			VFS_WriteAt(disk->fd, base+Offset, retLen, Buffer);
			if(bNewBlocks)	return Length;
			goto addBlocks;	// Ugh! A goto, but it seems unavoidable
		}
		
		// Write First Block
		VFS_WriteAt(disk->fd, base+Offset, disk->BlockSize-Offset, Buffer);
		Buffer += disk->BlockSize-Offset;
		retLen -= disk->BlockSize-Offset;
		block ++;
		
		// Write middle blocks
		while(retLen > disk->BlockSize)
		{
			base = Ext2_int_GetBlockAddr(disk, inode.i_block, block);
			VFS_WriteAt(disk->fd, base, disk->BlockSize, Buffer);
			Buffer += disk->BlockSize;
			retLen -= disk->BlockSize;
			block ++;
		}
		
		// Write last block
		base = Ext2_int_GetBlockAddr(disk, inode.i_block, block);
		VFS_WriteAt(disk->fd, base, retLen, Buffer);
		if(bNewBlocks)	return Length;	// Writing in only allocated space
	}
	
addBlocks:
	///\todo Implement block allocation
	WarningEx("EXT2", "File extending is not yet supported");
	
	return 0;
}

/**
 * \fn int Ext2_CloseFile(vfs_node *Node)
 * \brief Close a file (Remove it from the cache)
 */
int Ext2_CloseFile(tVFS_Node *Node)
{
	tExt2_Disk	*disk = Node->ImplPtr;
	inode_uncacheNode(disk->CacheID, Node->impl);
	return 1;
}

/**
 \fn char *Ext2_ReadDir(tVFS_Node *Node, int Pos)
 \brief Reads a directory entry
*/
char *Ext2_ReadDir(tVFS_Node *Node, int Pos)
{
	tExt2_Inode	inode;
	char	namebuf[EXT2_NAME_LEN+1];
	tExt2_DirEnt	dirent;
	Uint64	Base;	// Block's Base Address
	 int	block = 0, ofs = 0;
	 int	entNum = 0;
	tExt2_Disk	*disk = Node->ImplPtr;
	Uint64	vfsInode = 0;
	tVFS_Node	*retNode;
	Uint	size;
	
	ENTER("pNode iPos", Node, Pos);
	
	// Read directory's inode
	Ext2_int_GetInode(Node, &inode);
	size = inode.i_size;
	
	LOG("inode.i_block[0] = 0x%x\n", inode.i_block[0]);
	
	// Find Entry
	// Get First Block
	// - Do this ourselves as it is a simple operation
	Base = inode.i_block[0] * disk->BlockSize;
	while(Pos -- && size > 0)
	{
		VFS_ReadAt( disk->fd, Base+ofs, sizeof(tExt2_DirEnt), &dirent);
		ofs += dirent.rec_len;
		size -= dirent.rec_len;
		entNum ++;
		
		if(ofs >= disk->BlockSize) {
			block ++;
			if( ofs > disk->BlockSize ) {
				Warning("[EXT2] Directory Entry %i of inode %i ('%s') extends over a block boundary, ignoring\n",
					entNum-1, Node->impl, Node->name);
			}
			ofs = 0;
			Base = Ext2_int_GetBlockAddr( disk, inode.i_block, block );
		}
	}
	
	if(size <= 0)	return NULL;
	
	// Read Entry
	VFS_ReadAt( disk->fd, Base+ofs, sizeof(tExt2_DirEnt), &dirent );
	//LOG(" Ext2_ReadDir: dirent.inode = %i\n", dirent.inode);
	//LOG(" Ext2_ReadDir: dirent.rec_len = %i\n", dirent.rec_len);
	//LOG(" Ext2_ReadDir: dirent.name_len = %i\n", dirent.name_len);
	VFS_ReadAt( disk->fd, Base+ofs+sizeof(tExt2_DirEnt), dirent.name_len, namebuf );
	namebuf[ dirent.name_len ] = '\0';	// Cap off string
	
	
	// Ignore . and .. (these are done in the VFS)
	if( (namebuf[0] == '.' && namebuf[1] == '\0')
	||  (namebuf[0] == '.' && namebuf[1] == '.' && namebuf[2]=='\0'))
		LEAVE('p', VFS_SKIP);
		return VFS_SKIP;	// Skip
	}
	
	LEAVE('s', namebuf);
	// Create new node
	return strdup(namebuf);
}

/**
 \fn tVFS_Node *Ext2_FindDir(tVFS_Node *node, char *filename)
 \brief Gets information about a file
 \param node	vfs node - Parent Node
 \param filename	String - Name of file
 \return VFS Node of file
*/
tVFS_Node *Ext2_FindDir(tVFS_Node *Node, char *Filename)
{
	tExt2_Disk	*disk = Node->ImplPtr;
	tExt2_Inode	inode;
	char	namebuf[EXT2_NAME_LEN+1];
	tExt2_DirEnt	dirent;
	Uint64	Base;	// Block's Base Address
	 int	block = 0, ofs = 0;
	 int	entNum = 0;
	Uint	size;
	
	// Read directory's inode
	Ext2_int_GetInode(Node, &inode);
	size = inode.i_size;
	
	// Get First Block
	// - Do this ourselves as it is a simple operation
	Base = inode.i_block[0] * disk->BlockSize;
	// Find File
	while(size > 0)
	{
		VFS_ReadAt( disk->fd, Base+ofs, sizeof(tExt2_DirEnt), &dirent);
		VFS_ReadAt( disk->fd, Base+ofs+sizeof(tExt2_DirEnt), dirent.name_len, namebuf );
		namebuf[ dirent.name_len ] = '\0';	// Cap off string
		// If it matches, create a node and return it
		if(strcmp(namebuf, Filename) == 0)
			return Ext2_int_CreateNode( disk, dirent.inode, namebuf );
		// Increment pointers
		ofs += dirent.rec_len;
		size -= dirent.rec_len;
		entNum ++;
		
		// Check for end of block
		if(ofs >= disk->BlockSize) {
			block ++;
			if( ofs > disk->BlockSize ) {
				Warnin("[EXT2 ] Directory Entry %i of inode %i ('%s') extends over a block boundary, ignoring\n",
					entNum-1, Node->impl, Node->name);
			}
			ofs = 0;
			Base = Ext2_int_GetBlockAddr( disk, inode.i_block, block );
		}
	}
	
	return NULL;
}

/**
 * \fn tVFS_Node *Ext2_MkNod(tVFS_Node *Parent, char *Name, int Flags)
 * \brief Create a new node
 */
tVFS_Node *Ext2_MkNod(tVFS_Node *Parent, char *Name, int Flags)
{
	return 0;
}

//==================================
//=       INTERNAL FUNCTIONS       =
//==================================


/**
 \internal
 \fn int Ext2_int_GetInode(vfs_node *Node, tExt2_Inode *Inode)
 \brief Gets the inode descriptor for a node
 \param node	node to get the Inode of
 \param inode	Destination
*/
int Ext2_int_GetInode(tVFS_Node *Node, tExt2_Inode *Inode)
{
	return Ext2_int_ReadInode(Node->ImplPtr, Node->Inode, Inode);
}

/**
 * \fn vfs_node *Ext2_int_CreateNode(tExt2_Disk *Disk, Uint InodeID, char *Name)
 * \brief Create a new VFS Node
 */
tVFS_Node *Ext2_int_CreateNode(tExt2_Disk *Disk, Uint InodeID, char *Name)
{
	tExt2_Inode	inode;
	tVFS_Node	retNode;
	tVFS_Node	*tmpNode;
	
	if( !Ext2_int_ReadInode(Disk, InodeID, &inode) )
		return NULL;
	
	if( (tmpNode = inode_getCache(Disk->CacheID, InodeID)) )
		return tmpNode;
	
	
	// Set identifiers
	retNode.Inode = InodeID;
	retNode.ImplPtr = Disk;
	
	// Set file length
	retNode.Size = inode.i_size;
	
	// Set Access Permissions
	retNode.UID = inode.i_uid;
	retNode.GID = inode.i_gid;
	retNode.NumACLs = 3;
	retNode.ACLs = VFS_UnixToAcessACL(inode.i_mode & 0777, inode.i_uid, inode.i_gid);
	
	//  Set Function Pointers
	retNode.Read = Ext2_Read;
	retNode.Write = Ext2_Write;
	retNode.Close = Ext2_CloseFile;
	
	switch(inode.i_mode & EXT2_S_IFMT)
	{
	// Symbolic Link
	case EXT2_S_IFLNK:
		retNode.Flags = VFS_FFLAG_SYMLINK;
		break;
	// Regular File
	case EXT2_S_IFREG:
		retNode.flags = 0;
		break;
	// Directory
	case EXT2_S_IFDIR:
		retNode.ReadRir = Ext2_ReadDir;
		retNode.FindDir = Ext2_FindDir;
		retNode.MkNod = Ext2_MkNod;
		//retNode.Relink = Ext2_Relink;
		retNode.Flags = VFS_FFLAG_DIRECTORY;
		break;
	// Unknown, Write protect and hide it to be safe 
	default:
		retNode.flags = VFS_FFLAG_READONLY|VFS_FFLAG_HIDDEN;
		break;
	}
	
	// Check if the file should be hidden
	if(Name[0] == '.')	retNode.Flags |= VFS_FFLAG_HIDDEN;
	
	// Set Timestamps
	retNode.ATime = now();
	retNode.MTime = inode.i_mtime * 1000;
	retNode.CTime = inode.i_ctime * 1000;
	
	// Save in node cache and return saved node
	return Inode_CacheNode(Disk->CacheID, &retNode);
}

/**
 * \fn int Ext2_int_ReadInode(tExt2_Disk *Disk, Uint InodeId, tExt2_Inode *Inode)
 * \brief Read an inode into memory
 */
int Ext2_int_ReadInode(tExt2_Disk *Disk, Uint InodeId, tExt2_Inode *Inode)
{
	 int	group, subId;
	
	//LogF("Ext2_int_ReadInode: (Disk=%p, InodeId=%i, Inode=%p)\n", Disk, InodeId, Inode);
	
	if(InodeId == 0)	return 0;
	
	InodeId --;	// Inodes are numbered starting at 1
	
	group = InodeId / Disk->SuperBlock.s_inodes_per_group;
	subId = InodeId % Disk->SuperBlock.s_inodes_per_group;
	
	//LogF(" Ext2_int_ReadInode: group=%i, subId = %i\n", group, subId);
	
	//Seek to Block - Absolute
	vfs_seek(Disk->fd, Disk->Groups[group].bg_inode_table * Disk->BlockSize, SEEK_SET);
	//Seeek to inode - Relative
	vfs_seek(Disk->fd, sizeof(tExt2_Inode)*subId, SEEK_CUR);
	vfs_read(Disk->fd, sizeof(tExt2_Inode), Inode);
	return 1;
}

/**
 * \fn Uint64 Ext2_int_GetBlockAddr(tExt2_Disk *Disk, Uint32 *Blocks, int BlockNum)
 * \brief Get the address of a block from an inode's list
 * \param Disk	Disk information structure
 * \param Blocks	Pointer to an inode's block list
 * \param BlockNum	Block index in list
 */
Uint64 Ext2_int_GetBlockAddr(tExt2_Disk *Disk, Uint32 *Blocks, int BlockNum)
{
	Uint32	*iBlocks;
	// Direct Blocks
	if(BlockNum < 12)
		return (Uint64)Blocks[BlockNum] * Disk->BlockSize;
	
	// Single Indirect Blocks
	iBlocks = malloc( Disk->BlockSize );
	VFS_ReadAt(Disk->fd, (Uint64)Blocks[12]*Disk->BlockSize, Disk->BlockSize, iBlocks);
	
	BlockNum -= 12;
	if(BlockNum < 256) {
		BlockNum = iBlocks[BlockNum];
		free(iBlocks);
		return (Uint64)BlockNum * Disk->BlockSize;
	}
	
	// Double Indirect Blocks
	if(BlockNum < 256*256)
	{
		VFS_ReadAt(Disk->fd, (Uint64)Blocks[13]*Disk->BlockSize, Disk->BlockSize, iBlocks);
		VFS_ReadAt(Disk->fd, (Uint64)iBlocks[BlockNum/256]*Disk->BlockSize, Disk->BlockSize, iBlocks);
		BlockNum = iBlocks[BlockNum%256];
		free(iBlocks);
		return (Uint64)BlockNum * Disk->BlockSize;
	}
	// Triple Indirect Blocks
	VFS_ReadAt(Disk->fd, (Uint64)Blocks[14]*Disk->BlockSize, Disk->BlockSize, iBlocks);
	VFS_ReadAt(Disk->fd, (Uint64)iBlocks[BlockNum/(256*256)]*Disk->BlockSize, Disk->BlockSize, iBlocks);
	VFS_ReadAt(Disk->fd, (Uint64)iBlocks[(BlockNum/256)%256]*Disk->BlockSize, Disk->BlockSize, iBlocks);
	BlockNum = iBlocks[BlockNum%256];
	free(iBlocks);
	return (Uint64)BlockNum * Disk->BlockSize;
}