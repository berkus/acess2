/*
 * AcessOS Microkernel Version
 * acess.h
 */
#ifndef _ACESS_H
#define _ACESS_H
/**
 * \file acess.h
 * \brief Acess2 Kernel API Core
 */

#include <stddef.h>
#include <arch.h>

//! Pack a structure
#define PACKED	__attribute__((packed))
//! Mark a function as not returning
#define NORETURN	__attribute__((noreturn))
//! Mark a function that its return value should be used
#define WARN_UNUSED_RET	__attribute__((warn_unused_result))
//! Mark a function (or variable) as deprecated
#define DEPRECATED	__attribute__((deprecated))
//! Mark a parameter as unused
#define UNUSED(x)	UNUSED_##x __attribute__((unused))

/**
 * \name Boolean constants
 * \{
 */
#define TRUE	1
#define FALSE	0
/**
 * \}
 */

#include <stdarg.h>
#include "errno.h"

// --- Types ---
typedef Uint32	tPID;	//!< Process ID type
typedef Uint32	tPGID;	//!< Process Group ID type
typedef Uint32	tTID;	//!< Thread ID Type
typedef Uint32	tUID;	//!< User ID Type
typedef Uint32	tGID;	//!< Group ID Type
typedef Sint64	tTimestamp;	//!< Timestamp (miliseconds since 00:00 1 Jan 1970)
typedef Sint64	tTime;	//!< Same again
typedef struct sShortSpinlock	tShortSpinlock;	//!< Opaque (kinda) spinlock
typedef int	bool;	//!< Boolean type
typedef Uint64	off_t;	//!< VFS Offset

// --- Helper Macros ---
/**
 * \name Helper Macros
 * \{
 */
#define	CONCAT(x,y) x ## y
#define EXPAND_CONCAT(x,y) CONCAT(x,y)
#define STR(x) #x
#define EXPAND_STR(x) STR(x)

extern char	__buildnum[];
#define BUILD_NUM	((int)(Uint)&__buildnum)
extern const char	gsGitHash[];
extern const char	gsBuildInfo[];

#define VER2(major,minor)	((((major)&0xFF)<<8)|((minor)&0xFF))
/**
 * \}
 */

//! \brief Error number
#define errno	(*Threads_GetErrno())

// === CONSTANTS ===
// --- Memory Flags --
/**
 * \name Memory Flags
 * \{
 * \todo Move to mm_virt.h
 */
#define	MM_PFLAG_RO		0x01	// Writes disallowed
#define	MM_PFLAG_EXEC	0x02	// Allow execution
#define	MM_PFLAG_NOPAGE	0x04	// Prevent from being paged out
#define	MM_PFLAG_COW	0x08	// Copy-On-Write
#define	MM_PFLAG_KERNEL	0x10	// Kernel-Only (Ring0)
/**
 * \}
 */
// --- Interface Flags & Macros
/**
 * \name Flags for Proc_Clone
 * \{
 */
//! Clone the entire process
#define CLONE_VM	0x10
//! Don't copy user pages
#define CLONE_NOUSER	0x20
//! Inherit the parent's PGID
#define CLONE_PGID	0x40
/**
 * \}
 */

// === Types ===
/**
 * \brief Thread root function
 * 
 * Function pointer prototype of a thread entrypoint. When it
 * returns, Threads_Exit is called
 */
typedef void (*tThreadFunction)(void*);

// === Kernel Export Macros ===
/**
 * \name Kernel exports
 * \{
 */
//! Kernel symbol definition
typedef struct sKernelSymbol {
	const char	*Name;	//!< Symbolic name
	tVAddr	Value;	//!< Value of the symbol
} tKernelSymbol;
//! Export a pointer symbol (function/array)
#define	EXPORT(_name)	tKernelSymbol _kexp_##_name __attribute__((section ("KEXPORT"),unused))={#_name, (tVAddr)_name}
//! Export a variable
#define	EXPORTV(_name)	tKernelSymbol _kexp_##_name __attribute__((section ("KEXPORT"),unused))={#_name, (tVAddr)&_name}
//! Export a symbol under another name
#define	EXPORTAS(_sym,_name)	tKernelSymbol _kexp_##_name __attribute__((section ("KEXPORT"),unused))={#_name, (tVAddr)_sym}
/**
 * \}
 */

// === FUNCTIONS ===
// --- IRQs ---
/**
 * \name IRQ hander registration
 * \{
 */
extern int	IRQ_AddHandler(int Num, void (*Callback)(int, void*), void *Ptr);
extern void	IRQ_RemHandler(int Handle);
/**
 * \}
 */

#include "logdebug.h"

// --- IO ---
#if NO_IO_BUS
#define inb(a)	(Log_Panic("Arch", STR(ARCHDIR)" does not support in* (%s:%i)", __FILE__, __LINE__),0)
#define inw(a)	inb(a)
#define ind(a)	inb(a)
#define inq(a)	inb(a)
#define outb(a,b)	(Log_Panic("Arch", STR(ARCHDIR)" does not support out* (%s:%i)", __FILE__, __LINE__),(void)(b))
#define outw(a,b)	outb(a,b)
#define outd(a,b)	outb(a,b)
#define outq(a,b)	outb(a,b)
#else
/**
 * \name I/O Memory Access
 * \{
 */
extern void	outb(Uint16 Port, Uint8 Data);
extern void	outw(Uint16 Port, Uint16 Data);
extern void	outd(Uint16 Port, Uint32 Data);
extern void	outq(Uint16 Port, Uint64 Data);
extern Uint8	inb(Uint16 Port);
extern Uint16	inw(Uint16 Port);
extern Uint32	ind(Uint16 Port);
extern Uint64	inq(Uint16 Port);
/**
 * \}
 */
#endif
// --- Memory Management ---
/**
 * \name Memory Management
 * \{
 * \todo Move to mm_virt.h
 */
/**
 * \brief Allocate a physical page at \a VAddr
 * \param VAddr	Virtual Address to allocate at
 * \return Physical address allocated
 */
extern tPAddr	MM_Allocate(tVAddr VAddr) __attribute__ ((warn_unused_result));
/**
 * \brief Deallocate a page
 * \param VAddr	Virtual address to unmap
 */
extern void	MM_Deallocate(tVAddr VAddr);
/**
 * \brief Map a physical page at \a PAddr to \a VAddr
 * \param VAddr	Target virtual address
 * \param PAddr	Physical address to map
 * \return Boolean Success
 */
extern int	MM_Map(tVAddr VAddr, tPAddr PAddr);
/**
 * \brief Get the physical address of \a Addr
 * \param Addr	Address of the page to get the physical address of
 * \return Physical page mapped at \a Addr
 */
extern tPAddr	MM_GetPhysAddr(volatile const void *Addr);
/**
 * \brief Set the access flags on a page
 * \param VAddr	Virtual address of the page
 * \param Flags New flags value
 * \param Mask	Flags to set
 */
extern void	MM_SetFlags(tVAddr VAddr, Uint Flags, Uint Mask);
/**
 * \brief Get the flags on a flag
 * \param VAddr	Virtual address of page
 * \return Flags value of the page
 */
extern Uint	MM_GetFlags(tVAddr VAddr);
/**
 * \brief Checks is a memory range is user accessable
 * \param VAddr	Base address to check
 * \return 1 if the memory is all user-accessable, 0 otherwise
 */
#define MM_IsUser(VAddr)	(!(MM_GetFlags((tVAddr)(VAddr))&MM_PFLAG_KERNEL))
/**
 * \brief Temporarily map a page into the address space
 * \param PAddr	Physical addres to map
 * \return Virtual address of page in memory
 * \note There is only a limited ammount of slots avaliable
 */
extern void	*MM_MapTemp(tPAddr PAddr);
/**
 * \brief Free a temporarily mapped page
 * \param Ptr	Pointer to page base
 */
extern void	MM_FreeTemp(void *Ptr);
/**
 * \brief Map a physcal address range into the virtual address space
 * \param PAddr	Physical address to map in
 * \param Number	Number of pages to map
 */
extern void	*MM_MapHWPages(tPAddr PAddr, Uint Number);
/**
 * \brief Allocates DMA physical memory
 * \param Pages	Number of pages required
 * \param MaxBits	Maximum number of bits the physical address can have
 * \param PhysAddr	Pointer to the location to place the physical address allocated
 * \return Virtual address allocate
 */
extern void	*MM_AllocDMA(int Pages, int MaxBits, tPAddr *PhysAddr);
/**
 * \brief Unmaps an allocated hardware range
 * \param VAddr	Virtual address allocate by ::MM_MapHWPages or ::MM_AllocDMA
 * \param Number	Number of pages to free
 */
extern void	MM_UnmapHWPages(tVAddr VAddr, Uint Number);
/**
 * \brief Allocate a single physical page
 * \return Physical address allocated
 */
extern tPAddr	MM_AllocPhys(void);
/**
 * \brief Allocate a contiguous range of physical pages
 * \param Pages	Number of pages to allocate
 * \param MaxBits	Maximum number of address bits allowed
 * \return First physical address allocated
 */
extern tPAddr	MM_AllocPhysRange(int Pages, int MaxBits);
/**
 * \brief Reference a physical page
 * \param PAddr	Page to mark as referenced
 */
extern void	MM_RefPhys(tPAddr PAddr);
/**
 * \brief Dereference a physical page
 * \param PAddr	Page to dereference
 */
extern void	MM_DerefPhys(tPAddr PAddr);
/**
 * \brief Get the number of times a page has been referenced
 * \param PAddr	Address to check
 * \return Reference count for the page
 */
extern int	MM_GetRefCount(tPAddr PAddr);
/**
 * \brief Set the node associated with a page
 * \param PAddr	Physical address of page
 * \param Node	Node pointer (tVFS_Node)
 * \return Boolean failure
 * \retval 0	Success
 * \retval 1	Page not allocated
 */
extern int	MM_SetPageNode(tPAddr PAddr, void *Node);
/**
 * \brief Get the node associated with a page
 * \param PAddr	Physical address of page
 * \param Node	Node pointer (tVFS_Node) destination
 * \return Boolean failure
 * \retval 0	Success
 * \retval 1	Page not allocated
 */
extern int	MM_GetPageNode(tPAddr PAddr, void **Node);
/**
 * \}
 */

// --- Memory Manipulation ---
/**
 * \name Memory Manipulation
 * \{
 */
extern int	memcmp(const void *m1, const void *m2, size_t count);
extern void	*memcpy(void *dest, const void *src, size_t count);
extern void	*memcpyd(void *dest, const void *src, size_t count);
extern void	*memmove(void *dest, const void *src, size_t len);
extern void	*memset(void *dest, int val, size_t count);
extern void	*memsetd(void *dest, Uint32 val, size_t count);
/**
 * \}
 */
/**
 * \name Memory Validation
 * \{
 */
extern int	CheckString(const char *String);
extern int	CheckMem(const void *Mem, int Num);
/**
 * \}
 */

// --- Endianness ---
/**
 * \name Endianness Swapping
 * \{
 */
#ifdef __BIG_ENDIAN__
#define	LittleEndian16(_val)	SwapEndian16(_val)
#define	LittleEndian32(_val)	SwapEndian32(_val)
#define	LittleEndian64(_val)	SwapEndian32(_val)
#define	BigEndian16(_val)	(_val)
#define	BigEndian32(_val)	(_val)
#define	BigEndian64(_val)	(_val)
#else
#define	LittleEndian16(_val)	(_val)
#define	LittleEndian32(_val)	(_val)
#define	LittleEndian64(_val)	(_val)
#define	BigEndian16(_val)	SwapEndian16(_val)
#define	BigEndian32(_val)	SwapEndian32(_val)
#define	BigEndian64(_val)	SwapEndian64(_val)
#endif
extern Uint16	SwapEndian16(Uint16 Val);
extern Uint32	SwapEndian32(Uint32 Val);
extern Uint64	SwapEndian64(Uint64 Val);
/**
 * \}
 */

// --- Strings ---
#include <acess_string.h>

#include <ctype.h>

/**
 * \brief Get a random number
 * \return Random number
 * \note Current implementation is a linear congruency
 */
extern int	rand(void);
/**
 * \brief Call a function with a variable number of arguments
 * \param Function	Function address
 * \param NArgs	Number of entries in \a Args
 * \param Args	Array of arguments
 * \return Integer from called Function
 */
extern int	CallWithArgArray(void *Function, int NArgs, Uint *Args);

// --- Heap ---
#include <heap.h>
/**
 * \brief Magic heap allocation function
 */
extern void	*alloca(size_t Size);

// --- Modules ---
/**
 * \name Modules
 * \{
 */
extern int	Module_LoadMem(void *Buffer, Uint Length, const char *ArgStr);
extern int	Module_LoadFile(const char *Path, const char *ArgStr);
/**
 * \}
 */

// --- Timing ---
/**
 * \name Time and Timing
 * \{
 */
/**
 * \brief Create a timestamp from a time
 * \note Days/Months are zero-based (e.g. December is 11, and has a day range of 0-30)
 */
extern tTime	timestamp(int sec, int mins, int hrs, int day, int month, int year);
/**
 * \brief Extract the date/time from a timestamp
 * \note Days/Months are zero-based (e.g. December is 11, and has a day range of 0-30)
 */
extern void	format_date(tTime TS, int *year, int *month, int *day, int *hrs, int *mins, int *sec, int *ms);
/**
 * \brief Gets the current timestamp (miliseconds since Midnight 1st January 1970)
 */
extern Sint64	now(void);
/**
 * \}
 */

// --- Threads ---
/**
 * \name Threads and Processes
 * \{
 */
extern struct sThread	*Proc_SpawnWorker(void (*Fcn)(void*), void *Data);
extern int	Proc_Spawn(const char *Path);
extern int	Proc_SysSpawn(const char *Binary, const char **ArgV, const char **EnvP, int nFD, int *FDs);
extern int	Proc_Execve(const char *File, const char **ArgV, const char **EnvP, int DataSize);
extern void	Threads_Exit(int TID, int Status);
extern void	Threads_Yield(void);
extern void	Threads_Sleep(void);
extern int	Threads_WakeTID(tTID Thread);
extern tPGID	Threads_GetPGID(void);
extern tPID	Threads_GetPID(void);
extern tTID	Threads_GetTID(void);
extern tUID	Threads_GetUID(void);
extern tGID	Threads_GetGID(void);
extern int	SpawnTask(tThreadFunction Function, void *Arg);
extern int	*Threads_GetErrno(void);
extern int	Threads_SetName(const char *NewName);
/**
 * \}
 */

// --- Simple Math ---
//! Divide and round up
extern int	DivUp(int num, int dem);
//! Divide and Modulo 64-bit unsigned integer
extern Uint64	DivMod64U(Uint64 Num, Uint64 Den, Uint64 *Rem);

static inline int MIN(int a, int b) { return a < b ? a : b; }
static inline int MAX(int a, int b) { return a > b ? a : b; }

#include <binary_ext.h>
#include <vfs_ext.h>
#include <mutex.h>

#endif
