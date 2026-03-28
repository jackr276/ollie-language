/**
 * Author: Jack Robbins
 * _start is the true entry point for any given program on Linux. The start program
 * is what will actually invoke main for us
 */

.globl _start
_start:
	/* 0 out the frame pointer */
	xorq %rbp, %rbp

	/* Get argc off of the stack */
	movq (%rsp), %rsi

	/* Put argv(stack pointer) into %rdx */
	leaq 8(%rsp), %rdx

	/* Align the stack to 16 bytes */
	andq $-16, %rsp

	/* Push a fake return address(%rax) and a stack end */
	pushq %rax
	pushq %rsp

	/* Function pointer to main */
	leaq main(%rip), %rdi
	
	/* Invoke the actual call itself */
	call __ostl_start_main

	/* This is unreachable - if it somehow is reached then hlt causes a segfault */
	hlt
