/*
 * AcessOS LibC
 * stdlib.h
 */
#ifndef __STDLIB_H
#define __STDLIB_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#define EXIT_FAILURE	1
#define EXIT_SUCCESS	0

/* --- Spinlock Macros --- */
/* TODO: Support non-x86 architectures */
#define DEFLOCK(_name)	static int _spinlock_##_name=0;
#define LOCK(_name)	do{int v=1;while(v){__asm__ __volatile__("lock cmpxchgl %0, (%1)":"=a"(v):"D"((&_spinlock_##_name)),"a"(1));yield();}}while(0)
#define UNLOCK(_name) __asm__ __volatile__("lock andl $0, (%0)"::"D"(&_spinlock_##_name))

/* --- StdLib --- */
extern void	_exit(int code) __attribute__((noreturn));	/* NOTE: Also defined in acess/sys.h */

extern long long	strtoll(const char *ptr, char **end, int base);
extern long	strtol(const char *ptr, char **end, int base);
extern unsigned long long	strtoull(const char *ptr, char **end, int base);
extern unsigned long	strtoul(const char *ptr, char **end, int base);
extern int	atoi(const char *ptr);

extern double	strtod(const char *ptr, char **end);
extern float	strtof(const char *ptr, char **end);
extern float	atof(const char *ptr);

extern void	exit(int status) __attribute__((noreturn));
extern void	abort(void);
extern void	atexit(void (*__func)(void));
extern int	abs(int j);
extern long int	labs(long int j);
extern long long int	llabs(long long int j);

/* --- Environment --- */
extern char	*getenv(const char *name);

/* --- Search/Sort --- */
typedef int (*_stdlib_compar_t)(const void *, const void *);
extern void	*bsearch(const void *key, const void *base, size_t nmemb, size_t size, _stdlib_compar_t compar);
extern void	qsort(void *base, size_t nmemb, size_t size, _stdlib_compar_t compar);

/* --- Heap --- */
extern void	free(void *mem);
extern void	*malloc(size_t bytes);
extern void	*calloc(size_t __nmemb, size_t __size);
extern void	*realloc(void *__ptr, size_t __size);
extern int	IsHeap(void *ptr);

/* --- Random --- */
extern void	srand(unsigned int seed);
extern int	rand(void);
extern int	rand_p(unsigned int *seedp);

#ifndef SEEK_CUR
# define SEEK_CUR	0
# define SEEK_SET	1
# define SEEK_END	(-1)
#endif

#endif
