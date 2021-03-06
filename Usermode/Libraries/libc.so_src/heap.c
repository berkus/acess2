/*
AcessOS Basic LibC
heap.c - Heap Manager
*/
#include <acess/sys.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"

#if 0
# define DEBUGS(s...)	_SysDebug(s)
#else
# define DEBUGS(s...)	do{}while(0)
#endif

// === Constants ===
#define MAGIC	0xACE55051	//AcessOS1
#define MAGIC_FREE	(~MAGIC)
#define BLOCK_SIZE	16	//Minimum
#define HEAP_INIT_SIZE	0x10000

typedef unsigned int Uint;

// === TYPES ===
typedef struct {
	uint32_t	magic;
	size_t	size;
	char	data[];
}	heap_head;
typedef struct {
	heap_head	*header;
	uint32_t	magic;
}	heap_foot;

// === LOCAL VARIABLES ===
static void	*_heap_start = NULL;
static void	*_heap_end = NULL;

// === PROTOTYPES ===
EXPORT void	*malloc(size_t bytes);
EXPORT void	*calloc(size_t bytes, size_t count);
EXPORT void	free(void *mem);
EXPORT void	*realloc(void *mem, size_t bytes);
EXPORT void	*sbrk(int increment);
LOCAL void	*extendHeap(int bytes);
static void	*FindHeapBase();
LOCAL uint	brk(uintptr_t newpos);
LOCAL void	Heap_Dump(void);

//Code

/**
 \fn EXPORT void *malloc(size_t bytes)
 \brief Allocates memory from the heap space
 \param bytes	Integer - Size of buffer to return
 \return Pointer to buffer
*/
EXPORT void *malloc(size_t bytes)
{
	size_t	bestSize;
	size_t	closestMatch = 0;
	void	*bestMatchAddr = 0;
	heap_head	*curBlock;

//	_SysDebug("&_heap_start = %p, _heap_start = %p", &_heap_start, _heap_start);
	// Initialise Heap
	if(_heap_start == NULL)
	{
		_heap_start = sbrk(0);
		_heap_end = _heap_start;
		extendHeap(HEAP_INIT_SIZE);
	}
	
	curBlock = _heap_start;
//	_SysDebug("_heap_start = %p", _heap_start);
	
	bestSize = bytes + sizeof(heap_head) + sizeof(heap_foot) + BLOCK_SIZE - 1;
	bestSize = (bestSize/BLOCK_SIZE)*BLOCK_SIZE;	//Round up to block size
	
	while( (uintptr_t)curBlock < (uintptr_t)_heap_end)
	{
		//_SysDebug(" malloc: curBlock = 0x%x, curBlock->magic = 0x%x\n", curBlock, curBlock->magic);
		if(curBlock->magic == MAGIC_FREE)
		{
			if(curBlock->size == bestSize)
				break;
			if(bestSize < curBlock->size && (curBlock->size < closestMatch || closestMatch == 0)) {
				closestMatch = curBlock->size;
				bestMatchAddr = curBlock;
			}
		}
		else if(curBlock->magic != MAGIC)
		{
			//Corrupt Heap
			Heap_Dump();
			_SysDebug("malloc: Corrupt Heap\n");
			return NULL;
		}
		curBlock = (heap_head*)((uintptr_t)curBlock + curBlock->size);
	}
	
	if((uintptr_t)curBlock < (uintptr_t)_heap_start) {
		_SysDebug("malloc: Heap underrun for some reason\n");
		return NULL;
	}
	
	//Found a perfect match
	if((uintptr_t)curBlock < (uintptr_t)_heap_end) {
		curBlock->magic = MAGIC;
		return (void*)((uintptr_t)curBlock + sizeof(heap_head));
	}
	
	//Out of Heap Space
	if(!closestMatch) {
		curBlock = extendHeap(bestSize);	//Allocate more
		if(curBlock == NULL) {
			_SysDebug("malloc: Out of Heap Space\n");
			return NULL;
		}
		curBlock->magic = MAGIC;
		DEBUGS("malloc(0x%x) = %p (extend) 0x%x", bytes, curBlock->data, bestSize);
		return curBlock->data;
	}
	
	heap_head *besthead = (void*)bestMatchAddr;
	
	//Split Block?
	if(closestMatch - bestSize > BLOCK_SIZE) {
		heap_foot	*foot;
		curBlock = (heap_head*)bestMatchAddr;
		curBlock->magic = MAGIC;
		curBlock->size = bestSize;
		foot = (heap_foot*)(bestMatchAddr + bestSize - sizeof(heap_foot));
		foot->header = curBlock;
		foot->magic = MAGIC;

		curBlock = (heap_head*)(bestMatchAddr + bestSize);
		curBlock->magic = MAGIC_FREE;
		curBlock->size = closestMatch - bestSize;
		
		foot = (heap_foot*)(bestMatchAddr + closestMatch - sizeof(heap_foot));
		foot->header = curBlock;
		
		besthead->magic = MAGIC;	//mark as used
		DEBUGS("malloc(0x%x) = %p (split) 0x%x", bytes, besthead->data, bestSize);
		return besthead->data;
	}
	
	//Don't Split the block
	besthead->magic = MAGIC;
	DEBUGS("malloc(0x%x) = %p (reuse) 0x%x", bytes, besthead->data, besthead->size);
	return besthead->data;
}

/**
 * \fn EXPORT void *calloc(size_t bytes, size_t count)
 * \brief Allocate and zero a block of memory
 * \param __nmemb	Number of memeber elements
 * \param __size	Size of one element
 */
EXPORT void *calloc(size_t __nmemb, size_t __size)
{
	void	*ret = malloc(__size*__nmemb);
	if(!ret)	return NULL;
	memset(ret, 0, __size*__nmemb);
	return ret;
}

/**
 \fn EXPORT void free(void *mem)
 \brief Free previously allocated memory
 \param mem	Pointer - Memory to free
*/
EXPORT void free(void *mem)
{
	heap_head	*head = (void*)((intptr_t)mem-sizeof(heap_head));
	
	// Sanity please!
	if(!mem)	return;
	
	if(head->magic != MAGIC)	//Valid Heap Address
		return;
	
	head->magic = MAGIC_FREE;
	DEBUGS("free(%p) : 0x%x bytes", mem, head->size);
	
	//Unify Right
	if((uintptr_t)head + head->size < (uintptr_t)_heap_end)
	{
		heap_head	*nextHead = (heap_head*)((intptr_t)head + head->size);
		if(nextHead->magic == MAGIC_FREE) {	//Is the next block free
			head->size += nextHead->size;	//Amalgamate
			nextHead->magic = 0;	//For Security
		}
	}
	//Unify Left
	if((uintptr_t)head - sizeof(heap_foot) > (uintptr_t)_heap_start)
	{
		heap_head	*prevHead;
		heap_foot	*prevFoot = (heap_foot *)((intptr_t)head - sizeof(heap_foot));
		if(prevFoot->magic == MAGIC) {
			prevHead = prevFoot->header;
			if(prevHead->magic == MAGIC_FREE) {
				prevHead->size += head->size;	//Amalgamate
				head->magic = 0;	//For Security
			}
		}
	}
}

/**
 \fn EXPORT void *realloc(void *oldPos, size_t bytes)
 \brief Reallocate a block of memory
 \param bytes	Integer - Size of new buffer
 \param oldPos	Pointer - Old Buffer
 \return Pointer to new buffer
*/
EXPORT void *realloc(void *oldPos, size_t bytes)
{
	void *ret;
	heap_head	*head;
	
	if(oldPos == NULL) {
		return malloc(bytes);
	}
	
	//Check for free space after block
	head = (heap_head*)((uintptr_t)oldPos-sizeof(heap_head));
	
	//Hack to used free's amagamating algorithym and malloc's splitting
	free(oldPos);
	
	//Allocate new memory
	ret = malloc(bytes);
	if(ret == NULL)
		return NULL;
	
	//Copy Old Data
	if(ret != oldPos) {
		memcpy(ret, oldPos, head->size-sizeof(heap_head)-sizeof(heap_foot));
	}
	
	//Return
	return ret;
}

/**
 * \fn LOCAL void *extendHeap(int bytes)
 * \brief Create a new block at the end of the heap area
 * \param bytes	Integer - Size reqired
 * \return Pointer to last free block
 */

LOCAL void *extendHeap(int bytes)
{
	heap_head	*head = _heap_end;
	heap_foot	*foot;
	
	//Expand Memory Space  (Use foot as a temp pointer)
	foot = sbrk(bytes);
	if(foot == (void*)-1)
		return NULL;
	
	//Create New Block
	// Header
	head->magic = MAGIC_FREE;	//Unallocated
	head->size = bytes;
	// Footer
	foot = _heap_end + bytes - sizeof(heap_foot);
	foot->header = head;
	foot->magic = MAGIC;
	
	//Combine with previous block if nessasary
	if(_heap_end != _heap_start && ((heap_foot*)((uintptr_t)_heap_end-sizeof(heap_foot)))->magic == MAGIC) {
		heap_head	*tmpHead = ((heap_foot*)((uintptr_t)_heap_end-sizeof(heap_foot)))->header;
		if(tmpHead->magic == MAGIC_FREE) {
			tmpHead->size += bytes;
			foot->header = tmpHead;
			head = tmpHead;
		}
	}
	
	_heap_end = (void*) ((uintptr_t)foot+sizeof(heap_foot));
	return head;
}

/**
 \fn EXPORT void *sbrk(int increment)
 \brief Increases the program's memory space
 \param count	Integer - Size of heap increase
 \return Pointer to start of new region
*/
EXPORT void *sbrk(int increment)
{
	static uintptr_t oldEnd = 0;
	static uintptr_t curEnd = 0;

	//_SysDebug("sbrk: (increment=%i)", increment);

	if (curEnd == 0) {
		oldEnd = curEnd = (uintptr_t)FindHeapBase();
		//_SysAllocate(curEnd);	// Allocate the first page
	}

	//_SysDebug(" sbrk: oldEnd = 0x%x", oldEnd);
	if (increment == 0)	return (void *) curEnd;

	oldEnd = curEnd;

	// Single Page
	if( (curEnd & 0xFFF) && (curEnd & 0xFFF) + increment < 0x1000 )
	{
		//if( curEnd & 0xFFF == 0 )
		//{
		//	if( !_SysAllocate(curEnd) )
		//	{
		//		_SysDebug("sbrk - Error allocating memory");
		//		return (void*)-1;
		//	}
		//}
		curEnd += increment;
		//_SysDebug("sbrk: RETURN %p (single page, no alloc)", (void *) oldEnd);
		return (void *)oldEnd;
	}

	increment -= curEnd & 0xFFF;
	curEnd += 0xFFF;	curEnd &= ~0xFFF;
	while( increment > 0 )
	{
		if( !_SysAllocate(curEnd) )
		{
			// Error?
			_SysDebug("sbrk - Error allocating memory");
			return (void*)-1;
		}
		increment -= 0x1000;
		curEnd += 0x1000;
	}

	//_SysDebug("sbrk: RETURN %p", (void *) oldEnd);
	return (void *) oldEnd;
}

/**
 * \fn EXPORT int IsHeap(void *ptr)
 */
EXPORT int IsHeap(void *ptr)
{
	#if 0
	heap_head	*head;
	heap_foot	*foot;
	#endif
	if( (uintptr_t)ptr < (uintptr_t)_heap_start )	return 0;
	if( (uintptr_t)ptr > (uintptr_t)_heap_end )	return 0;
	
	#if 0
	head = (void*)((Uint)ptr - 4);
	if( head->magic != MAGIC )	return 0;
	foot = (void*)( (Uint)ptr + head->size - sizeof(heap_foot) );
	if( foot->magic != MAGIC )	return 0;
	#endif
	return 1;
}

// === STATIC FUNCTIONS ===
/**
 * Does the job of brk(0)
 */
static void *FindHeapBase()
{
	#if 0
	#define MAX		0xC0000000	// Address
	#define THRESHOLD	512	// Pages
	uint	addr;
	uint	stretch = 0;
	uint64_t	tmp;
	
	// Scan address space
	for(addr = 0;
		addr < MAX;
		addr += 0x1000
		)
	{
		tmp = _SysGetPhys(addr);
		if( tmp != 0 ) {
			stretch = 0;
		} else {
			stretch ++;
			if(stretch > THRESHOLD)
			{
				return (void*)( addr - stretch*0x1000 );
			}
		}
		//__asm__ __volatile__ (
		//	"push %%ebx;mov %%edx,%%ebx;int $0xAC;pop %%ebx"
		//	::"a"(256),"d"("%x"),"c"(addr));
	}
	
	return NULL;
	#else
	return (void*)0x00900000;
	#endif
}

LOCAL uint brk(uintptr_t newpos)
{
	static uintptr_t	curpos;
	uint	pages;
	uint	ret = curpos;
	 int	delta;
	
	_SysDebug("brk: (newpos=0x%x)", newpos);
	
	// Find initial position
	if(curpos == 0)	curpos = (uintptr_t)FindHeapBase();
	
	// Get Current Position
	if(newpos == 0)	return curpos;
	
	if(newpos < curpos)	return newpos;
	
	delta = newpos - curpos;
	_SysDebug(" brk: delta = 0x%x", delta);
	
	// Do we need to add pages
	if(curpos & 0xFFF && (curpos & 0xFFF) + delta < 0x1000)
		return curpos += delta;
	
	// Page align current position
	if(curpos & 0xFFF)	delta -= 0x1000 - (curpos & 0xFFF);
	curpos = (curpos + 0xFFF) & ~0xFFF;
	
	// Allocate Pages
	pages = (delta + 0xFFF) >> 12;
	while(pages--)
	{
		_SysAllocate(curpos);
		curpos += 0x1000;
		delta -= 0x1000;
	}
	
	// Bring the current position to exactly what we want
	curpos -= ((delta + 0xFFF) & ~0xFFF) - delta;
	
	return ret;	// Return old curpos
}

void Heap_Dump(void)
{
	heap_head *cur = _heap_start;
	while( cur < (heap_head*)_heap_end )
	{
		switch( cur->magic )
		{
		case MAGIC:
			_SysDebug("Used block %p[0x%x] - ptr=%p", cur, cur->size, cur->data);
			break;
		case MAGIC_FREE:
			_SysDebug("Free block %p[0x%x] - ptr=%p", cur, cur->size, cur->data);
			break;
		default:
			_SysDebug("Block %p bad magic (0x%x)", cur, cur->magic);
			return ;
		}
		cur = (void*)( (char*)cur + cur->size );
	}
}

