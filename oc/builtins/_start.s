/**
 * Author: Jack Robbins
 * _start is the true entry point for any given program on Linux. The start program
 * is what will actually invoke main for us
 */

_start:
	/* 0 out the frame pointer */
	xorq %rbp, %rbp

	/* Get argc off of the stack */
	popq %rsi

	/* Put argv(stack pointer) into %rdx */
	movq %rsp, %rdx

	/* Align the stack to 16 bytes */
	andq $-16, %rsp

	/**
	* Push a fake return address(%rax) and a stack end
	*/
	pushq %rax
	pushq %rsp

	/* Function pointer to main */
	leaq main(%rip), %rdi

	//TODO CALL STAT MAIN

	hlt
