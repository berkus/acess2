/*
 */
#ifndef _THREADS_H_
#define _THREADS_H_

#include <arch.h>
#include <signal.h>
#include <proc.h>

/**
 * \brief IPC Message
 */
typedef struct sMessage
{
	struct sMessage	*Next;	//!< Next message in thread's inbox
	tTID	Source;	//!< Source thread ID
	Uint	Length;	//!< Length of message data in bytes
	Uint8	Data[];	//!< Message data
} tMsg;

/**
 * \brief Core threading structure
 * 
 */
typedef struct sThread
{
	// --- threads.c's
	/**
	 * \brief Next thread in current list
	 * \note Required to be first for linked list hacks to work
	 */
	struct sThread	*Next;
	struct sThread	*GlobalNext;	//!< Next thread in global list
	struct sThread	*GlobalPrev;	//!< Previous thread in global list
	tShortSpinlock	IsLocked;	//!< Thread's spinlock
	volatile int	Status;		//!< Thread Status
	void	*WaitPointer;	//!< What (Mutex/Thread/other) is the thread waiting on
	 int	RetStatus;	//!< Return Status
	
	Uint	TID;	//!< Thread ID
	Uint	TGID;	//!< Thread Group (Process)
	struct sThread	*Parent;	//!< Parent Thread
	Uint	UID, GID;	//!< User and Group
	char	*ThreadName;	//!< Name of thread
	
	// --- arch/proc.c's responsibility
	//! Kernel Stack Base
	tVAddr	KernelStack;
	
	//! Memory Manager State
	tMemoryState	MemState;
	
	//! State on task switch
	tTaskState	SavedState;
	
	// --- threads.c's
	 int	CurFaultNum;	//!< Current fault number, 0: none
	tVAddr	FaultHandler;	//!< Fault Handler
	
	tMsg * volatile	Messages;	//!< Message Queue
	tMsg	*LastMessage;	//!< Last Message (speeds up insertion)
	
	 int	Quantum, Remaining;	//!< Quantum Size and remaining timesteps
	 int	Priority;	//!< Priority - 0: Realtime, higher means less time
	
	Uint	Config[NUM_CFG_ENTRIES];	//!< Per-process configuration
	
	volatile int	CurCPU;
} tThread;


enum {
	THREAD_STAT_NULL,	// Invalid process
	THREAD_STAT_ACTIVE,	// Running and schedulable process
	THREAD_STAT_SLEEPING,	// Message Sleep
	THREAD_STAT_MUTEXSLEEP,	// Mutex Sleep
	THREAD_STAT_SEMAPHORESLEEP,	// Semaphore Sleep
	THREAD_STAT_WAITING,	// ??? (Waiting for a thread)
	THREAD_STAT_PREINIT,	// Being created
	THREAD_STAT_ZOMBIE,	// Died/Killed, but parent not informed
	THREAD_STAT_DEAD,	// Awaiting burial (free)
	THREAD_STAT_BURIED	// If it's still on the list here, something's wrong
};
static const char * const casTHREAD_STAT[] = {
	"THREAD_STAT_NULL",
	"THREAD_STAT_ACTIVE",
	"THREAD_STAT_SLEEPING",
	"THREAD_STAT_MUTEXSLEEP",
	"THREAD_STAT_SEMAPHORESLEEP",
	"THREAD_STAT_WAITING",
	"THREAD_STAT_PREINIT",
	"THREAD_STAT_ZOMBIE",
	"THREAD_STAT_DEAD",
	"THREAD_STAT_BURIED"
};

enum eFaultNumbers
{
	FAULT_MISC,
	FAULT_PAGE,
	FAULT_ACCESS,
	FAULT_DIV0,
	FAULT_OPCODE,
	FAULT_FLOAT
};

#define GETMSG_IGNORE	((void*)-1)

// === GLOBALS ===
extern BOOL	gaThreads_NoTaskSwitch[MAX_CPUS];

// === FUNCTIONS ===
extern tThread	*Proc_GetCurThread(void);
extern tThread	*Threads_GetThread(Uint TID);
extern void	Threads_SetPriority(tThread *Thread, int Pri);
extern int	Threads_Wake(tThread *Thread);
extern void	Threads_Kill(tThread *Thread, int Status);
extern void	Threads_AddActive(tThread *Thread);
extern tThread	*Threads_RemActive(void);
extern tThread	*Threads_GetNextToRun(int CPU, tThread *Last);

extern void	Threads_SetFaultHandler(Uint Handler);

extern int	Threads_SetUID(Uint *Errno, tUID ID);
extern int	Threads_SetGID(Uint *Errno, tUID ID);
extern int	Threads_WaitTID(int TID, int *Status);

extern tThread	*Threads_CloneTCB(Uint *Err, Uint Flags);
extern tThread	*Threads_CloneThreadZero(void);

extern int	Proc_SendMessage(Uint *Err, Uint Dest, int Length, void *Data);
extern int	Proc_GetMessage(Uint *Err, Uint *Source, void *Buffer);

#endif
