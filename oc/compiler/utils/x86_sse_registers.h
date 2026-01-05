/**
 * Author: Jack Robbins
 * This header file defines all x86 SSE register types as an enum
*/

#ifndef X86_SSE_REGISTERS_H
#define X86_SSE_REGISTERS_H

/**
 * Define the standard x86-64 SSE register set. There are 16 SSE registers(XMM0-15), and they
 * are all present for us to use
 */
typedef enum{
	NO_REG_SSE = 0, //Default is that there's no register used
	XMM0,
	XMM1,
	XMM2,
	XMM3,
	XMM4,
	XMM5,
	XMM6,
	XMM7,
	XMM8,
	XMM9,
	XMM10,
	XMM11,
	XMM12,
	XMM13,
	XMM14,
	XMM15
} sse_register_t;

#endif /* X86_SSE_REGISTERS_H */
