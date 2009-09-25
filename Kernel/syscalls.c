/*
 * AcessOS Microkernel Version
 * syscalls.c
 */
#define DEBUG	0

#include <common.h>
#include <syscalls.h>
#include <proc.h>
#include <errno.h>

// === IMPORTS ===
extern int	Proc_Clone(Uint *Err, Uint Flags);
extern int	Threads_WaitTID(int TID, int *status);
extern Uint	Proc_SendMessage(Uint *Err, Uint Dest, Uint Length, void *Data);
extern int	Proc_GetMessage(Uint *Err, Uint *Source, void *Buffer);
extern int	Proc_Execve(char *File, char **ArgV, char **EnvP);
extern Uint	Binary_Load(char *file, Uint *entryPoint);
extern int	VFS_FInfo(int FD, void *Dest, int MaxACLs);
extern int	Threads_SetName(char *NewName);
extern int	Threads_GetPID();
extern int	Threads_GetTID();
extern int	Threads_GetUID();
extern int	Threads_GetGID();

// === CODE ===
void SyscallHandler(tSyscallRegs *Regs)
{
	Uint64	ret = 0;
	Uint	err = 0;
	#if DEBUG
	ENTER("iThread iNum", gCurrentThread->TID, Regs->Num);
	if(Regs->Num < NUM_SYSCALLS)
		LOG("Syscall %s", cSYSCALL_NAMES[Regs->Num]);
	LOG("Arg1: 0x%x, Arg2: 0x%x, Arg3: 0x%x", Regs->Arg1, Regs->Arg2, Regs->Arg3);
	#endif
	switch(Regs->Num)
	{
	// -- Exit the current thread
	case SYS_EXIT:	Threads_Exit();	break;
	
	// -- Put the current thread to sleep
	case SYS_SLEEP:	Threads_Sleep();	break;
	
	// -- Yield current timeslice
	case SYS_YIELD:	Threads_Yield();	break;
	
	// -- Clone the current thread
	case SYS_CLONE:
		// Call clone system call
		ret = Proc_Clone(&err, Regs->Arg1);
		// Change user stack if requested
		if(ret == 0 && Regs->Arg2)
			Regs->StackPointer = Regs->Arg2;
		break;
	
	// -- Send a signal
	case SYS_KILL:
		err = -ENOSYS;
		ret = -1;
		break;
	
	// -- Wait for a thread
	case SYS_WAITTID:
		ret = Threads_WaitTID(Regs->Arg1, (void*)Regs->Arg2);
		break;
	
	case SYS_GETPHYS:
		ret = MM_GetPhysAddr(Regs->Arg1);
		break;
	// -- Map an address
	case SYS_MAP:	MM_Map(Regs->Arg1, Regs->Arg2);	break;
	// -- Allocate an address
	case SYS_ALLOCATE:	ret = MM_Allocate(Regs->Arg1);	break;
	// -- Unmap an address
	case SYS_UNMAP:		MM_Deallocate(Regs->Arg1);	break;
	
	// -- Get Thread/Process IDs
	case SYS_GETTID:	ret = Threads_GetTID();	break;
	case SYS_GETPID:	ret = Threads_GetPID();	break;
	// -- Get User/Group IDs
	case SYS_GETUID:	ret = Threads_GetUID();	break;
	case SYS_GETGID:	ret = Threads_GetGID();	break;
	
	// -- Send Message
	case SYS_SENDMSG:
		ret = Proc_SendMessage(&err, Regs->Arg1, Regs->Arg2, (void*)Regs->Arg3);
		break;
	// -- Check for messages
	case SYS_GETMSG:
		ret = Proc_GetMessage(&err, (Uint*)Regs->Arg1, (void*)Regs->Arg2);
		break;
	
	// -- Set the thread's name
	case SYS_SETNAME:
		// Sanity Check
		if(!Regs->Arg1) {	ret = -1;	err = -EINVAL;	break;	}
		Threads_SetName( (void*)Regs->Arg1 );
		break;
	
	// ---
	// Binary Control
	// ---
	case SYS_EXECVE:
		ret = Proc_Execve((char*)Regs->Arg1, (char**)Regs->Arg2, (char**)Regs->Arg3);
		break;
	case SYS_LOADBIN:
		ret = Binary_Load((char*)Regs->Arg1, (Uint*)Regs->Arg2);
		break;
	
	// ---
	// Virtual Filesystem
	// ---
	case SYS_OPEN:
		ret = VFS_Open((char*)Regs->Arg1, Regs->Arg2 | VFS_OPENFLAG_USER);
		break;
	
	case SYS_CLOSE:
		VFS_Close( Regs->Arg1 );
		break;
	
	case SYS_WRITE:
		#if BITS < 64
		VFS_Write( Regs->Arg1, Regs->Arg2|((Uint64)Regs->Arg3<<32), (void*)Regs->Arg4 );
		#else
		VFS_Write( Regs->Arg1, Regs->Arg2, (void*)Regs->Arg3 );
		#endif
		break;
	
	case SYS_READ:
		#if BITS < 64
		VFS_Read( Regs->Arg1, Regs->Arg2|((Uint64)Regs->Arg3<<32), (void*)Regs->Arg4 );
		#else
		VFS_Read( Regs->Arg1, Regs->Arg2, (void*)Regs->Arg3 );
		#endif
		break;
	
	case SYS_FINFO:
		ret = VFS_FInfo( Regs->Arg1, (void*)Regs->Arg2, Regs->Arg3 );
		break;
	
	// -- Debug
	case SYS_DEBUG:
		Log((char*)Regs->Arg1,
			Regs->Arg2, Regs->Arg3, Regs->Arg4, Regs->Arg5, Regs->Arg6);
		break;
	
	// -- Default (Return Error)
	default:
		Warning("SyscallHandler: Unknown System Call %i", Regs->Num);
		if(Regs->Num < NUM_SYSCALLS)
			Warning(" Syscall '%s'", cSYSCALL_NAMES[Regs->Num]);
		err = -ENOSYS;
		ret = -1;
		break;
	}
	#if BITS < 64
	Regs->Return = ret&0xFFFFFFFF;
	Regs->RetHi = ret >> 32;
	#else
	Regs->Return = ret;
	#endif
	Regs->Error = err;
	#if DEBUG
	LOG("SyscallHandler: err = %i", err);
	LEAVE('x', ret);
	#endif
}

