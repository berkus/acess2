/*
 * Acess2 ARMv7
 * - By John Hodge (thePowersGang)
 *
 * arch/arm7/include/assembly.h
 * - Assembly specific macros
 */
#ifndef _ASSEMBLY_H_
#define _ASSEMBLY_H_

#define PUSH_GPRS \
	str r0, [sp,#-1*4];\
	str r1, [sp,#-2*4];\
	str r2, [sp,#-3*4];\
	str r3, [sp,#-4*4];\
	str r4, [sp,#-5*4];\
	str r5, [sp,#-6*4];\
	str r6, [sp,#-7*4];\
	str r7, [sp,#-8*4];\
	str r8, [sp,#-9*4];\
	str r9, [sp,#-10*4];\
	str r10, [sp,#-11*4];\
	str r11, [sp,#-12*4];\
	str r12, [sp,#-13*4];\
	str sp, [sp,#-14*4];\
	str lr, [sp,#-15*4];\
	sub sp, #16*4

#define POP_GPRS add sp, #16*4; \
	ldr r0, [sp,#-1*4]; \
	ldr r1, [sp,#-2*4]; \
	ldr r2, [sp,#-3*4]; \
	ldr r3, [sp,#-4*4]; \
	ldr r4, [sp,#-5*4]; \
	ldr r5, [sp,#-6*4]; \
	ldr r6, [sp,#-7*4]; \
	ldr r7, [sp,#-8*4]; \
	ldr r8, [sp,#-9*4]; \
	ldr r9, [sp,#-10*4]; \
	ldr r10, [sp,#-11*4]; \
	ldr r11, [sp,#-12*4]; \
	ldr r12, [sp,#-13*4]; \
	ldr lr, [sp,#-15*4];

#endif

