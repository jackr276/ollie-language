	.file	"__ostl_start_main.s"
	.text
	.type ostl_start_main_exit, @function
ostl_start_main_exit:
	movq $231, %rax
syscall
.L5:
	jmp .L5
	.text
	.globl __ostl_start_main
	.type __ostl_start_main, @function
__ostl_start_main:
	subq $8, %rsp
	movq %rdi, %rax
	leaq 8(%rdx, %rsi, 8), %rcx
	movl %esi, %edi
	movq %rdx, %rsi
	movq %rcx, %rdx
	call *%rax /* --> %eax */
	movl %eax, %edi
	call ostl_start_main_exit /* --> void */
	movl $0, %eax
	addq $8, %rsp
	ret /* --> %eax */
