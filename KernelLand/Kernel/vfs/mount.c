/* 
 * Acess Micro - VFS Server version 1
 */
#include <acess.h>
#include <vfs.h>
#include <vfs_int.h>
#include <fs_sysfs.h>

// === IMPORTS ===
extern int	giVFS_MountFileID;
extern char	*gsVFS_MountFile;

// === PROTOTYPES ===
#if 0
 int	VFS_Mount(const char *Device, const char *MountPoint, const char *Filesystem, const char *Options);
#endif
void	VFS_UpdateMountFile(void);

// === GLOBALS ===
tMutex	glVFS_MountList;
tVFS_Mount	*gVFS_Mounts;
tVFS_Mount	*gVFS_RootMount = NULL;
Uint32	giVFS_NextMountIdent = 1;

// === CODE ===
/**
 * \brief Mount a device
 * \param Device	Device string to mount
 * \param MountPoint	Destination for the mount
 * \param Filesystem	Filesystem to use for the mount
 * \param Options		Options to be passed to the filesystem
 * \return -1 on Invalid FS, -2 on No Mem, 0 on success
 * 
 * Mounts the filesystem on \a Device at \a MountPoint using the driver
 * \a Filesystem. The options in the string \a Options is passed to the
 * driver's mount.
 */
int VFS_Mount(const char *Device, const char *MountPoint, const char *Filesystem, const char *Options)
{
	tVFS_Mount	*mnt;
	tVFS_Driver	*fs;
	 int	deviceLen = strlen(Device);
	 int	mountLen = strlen(MountPoint);
	 int	argLen = strlen(Options);
	
	// Get the filesystem
	fs = VFS_GetFSByName(Filesystem);
	if(!fs) {
		Log_Warning("VFS", "VFS_Mount - Unknown FS Type '%s'", Filesystem);
		return -1;
	}
	
	// Create mount information
	mnt = malloc( sizeof(tVFS_Mount)+deviceLen+1+mountLen+1+argLen+1 );
	if(!mnt) {
		return -2;
	}
	
	// HACK: Forces VFS_ParsePath to fall back on root  
	if(mountLen == 1 && MountPoint[0] == '/')
		mnt->MountPointLen = 0;
	else
		mnt->MountPointLen = mountLen;
	
	// Fill Structure
	mnt->Filesystem = fs;
	
	mnt->Device = &mnt->StrData[0];
	memcpy( mnt->Device, Device, deviceLen+1 );
	
	mnt->MountPoint = &mnt->StrData[deviceLen+1];
	memcpy( mnt->MountPoint, MountPoint, mountLen+1 );
	
	mnt->Options = &mnt->StrData[deviceLen+1+mountLen+1];
	memcpy( mnt->Options, Options, argLen+1 );
	
	// Initialise Volume
	mnt->RootNode = fs->InitDevice(Device, NULL);	//&ArgString);
	if(!mnt->RootNode) {
		free(mnt);
		return -2;
	}

	mnt->Identifier = giVFS_NextMountIdent++;
	#if 0
	// Ensure identifiers don't repeat
	// - Only a problem if there have been 4 billion mounts
	while( giVFS_NextMountIdent == 0 || VFS_GetMountByIdent(giVFS_NextMountIdent) )
		giVFS_NextMountIdent ++;
	#endif
	
	// Set root
	if(!gVFS_RootMount)	gVFS_RootMount = mnt;
	
	// Add to mount list
	Mutex_Acquire( &glVFS_MountList );
	{
		tVFS_Mount	*tmp;
		mnt->Next = NULL;
		if(gVFS_Mounts) {
			for( tmp = gVFS_Mounts; tmp->Next; tmp = tmp->Next );
			tmp->Next = mnt;
		}
		else {
			gVFS_Mounts = mnt;
		}
	}
	Mutex_Release( &glVFS_MountList );
	
	Log_Log("VFS", "Mounted '%s' to '%s' ('%s')", Device, MountPoint, Filesystem);
	
	VFS_UpdateMountFile();
	
	return 0;
}

/**
 * \brief Gets a mount point given the identifier
 */
tVFS_Mount *VFS_GetMountByIdent(Uint32 MountID)
{
	tVFS_Mount	*mnt;
	for(mnt = gVFS_Mounts; mnt; mnt = mnt->Next)
	{
		if(mnt->Identifier == MountID)
			return mnt;
	}
	return NULL;
}

/**
 * \brief Updates the mount file buffer
 * 
 * Updates the ProcFS mounts file buffer to match the current mounts list.
 */
void VFS_UpdateMountFile(void)
{
	 int	len = 0;
	char	*buf;
	tVFS_Mount	*mnt;
	
	// Format:
	// <device>\t<location>\t<type>\t<options>\n
	
	for(mnt = gVFS_Mounts; mnt; mnt = mnt->Next)
	{
		len += 4 + strlen(mnt->Device) + strlen(mnt->MountPoint)
			+ strlen(mnt->Filesystem->Name) + strlen(mnt->Options);
	}
	
	buf = malloc( len + 1 );
	len = 0;
	for(mnt = gVFS_Mounts; mnt; mnt = mnt->Next)
	{
		strcpy( &buf[len], mnt->Device );
		len += strlen(mnt->Device);
		buf[len++] = '\t';
		
		strcpy( &buf[len], mnt->MountPoint );
		len += strlen(mnt->MountPoint);
		buf[len++] = '\t';
		
		strcpy( &buf[len], mnt->Filesystem->Name );
		len += strlen(mnt->Filesystem->Name);
		buf[len++] = '\t';
		
		strcpy( &buf[len], mnt->Options );
		len += strlen(mnt->Options);
		buf[len++] = '\n';
	}
	buf[len] = 0;
	
	SysFS_UpdateFile( giVFS_MountFileID, buf, len );
	if( gsVFS_MountFile )	free( gsVFS_MountFile );
	gsVFS_MountFile = buf;
}