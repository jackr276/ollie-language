	.file	"./oc/builtins/precompiled_builtins/__ostl_start_main.s"
	.text
	.type ostl_start_main_exit, @function
ostl_start_main_exit:
	movq $231, %rax
	syscall
	ret
	.text
	.globl __ostl_start_main
	.type __ostl_start_main, @function
__ostl_start_main:
	movq %rdi, %rcx
	movl %esi, %eax
	addl $1, %eax
	leaq ( , %eax, 8), %rax
	leaq (%rdx, %rax), %rax
	movl %esi, %edi
	movq %rdx, %rsi
	movq %rax, %rdx
	call *%rcx /* --> %eax */
	movl %eax, %edi
	call ostl_start_main_exit /* --> void */
	movl $0, %eax
	ret /* --> %eax */
