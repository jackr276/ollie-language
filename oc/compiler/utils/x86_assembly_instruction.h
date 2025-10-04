/**
 * Author: Jack Robbins
 * This header file contains the definitions for all x86-64 instructions that OC
 * produces
*/

#ifndef X86_ASSEMBLY_INSTRUCTION_H
#define X86_ASSEMBLY_INSTRUCTION_H

/**
 * All x86-64 instructions that Ollie recognizes
 */
typedef enum{
	NO_INSTRUCTION_SELECTED = 0, //The NONE instruction, this is our default and we'll get this when we calloc
	PHI_FUNCTION, //Not really an instruction, but we still need to account for these
	RET,
	CALL,
	INDIRECT_CALL, //For function pointers
	MOVB,
	MOVW, //Regular register-to-register or immediate to register
	MOVL,
	MOVQ,
	MOVSX, //Move with sign extension from small to large register
	MOVZX, //Move with zero extension from small to large register
	REG_TO_MEM_MOVB,
	REG_TO_MEM_MOVW,
	REG_TO_MEM_MOVL,
	REG_TO_MEM_MOVQ,
	MEM_TO_REG_MOVB,
	MEM_TO_REG_MOVW,
	MEM_TO_REG_MOVL,
	MEM_TO_REG_MOVQ,
	LEAW,
	LEAL,
	LEAQ,
	INDIRECT_JMP, //For our switch statements
	CQTO, //convert quad-to-octa word
	CLTD, //convert long-to-double-long(quad)
	CWTL, //Convert word to long word
	CBTW, //Convert byte to word
	NOP,
	JMP, //Unconditional jump
	JNE, //Jump not equal
	JE, //Jump if equal
	JNZ, //Jump if not zero
	JZ, //Jump if zero
	JGE, //Jump GE(SIGNED)
	JG, //Jump GT(SIGNED)
	JLE, //Jump LE(SIGNED)
	JL, //JUMP LT(SIGNED)
	JA, //JUMP GT(UNSIGNED)
	JAE, //JUMP GE(UNSIGNED)
	JB, //JUMP LT(UNSIGNED)
	JBE, //JUMP LE(UNSIGNED)
	ADDB,
	ADDW,
	ADDL,
	ADDQ,
	MULB,
	MULW,
	MULL,
	MULQ,
	IMULB,
	IMULW,
	IMULL,
	IMULQ,
	DIVB,
	DIVW,
	DIVL,
	DIVQ,
	IDIVB,
	IDIVW,
	IDIVL,
	IDIVQ,
	IDIVB_FOR_MOD,
	IDIVW_FOR_MOD,
	IDIVL_FOR_MOD,
	IDIVQ_FOR_MOD,
	DIVB_FOR_MOD,
	DIVW_FOR_MOD,
	DIVL_FOR_MOD,
	DIVQ_FOR_MOD,
	SUBB,
	SUBW,
	SUBL,
	SUBQ,
	ASM_INLINE, //ASM inline statements aren't really instructions
	SHRB,
	SHRW,
	SHRL,
	SHRQ, 
	SARB,
	SARW,
	SARL, //Signed shift
	SARQ, //Signed shift
	SALW,
	SALB,
	SALL, //Signed shift 
	SALQ, //Signed shift
	SHLB,
	SHLW,
	SHLL,
	SHLQ,
	INCB,
	INCW,
	INCL,
	INCQ,
	DECB,
	DECW,
	DECL,
	DECQ,
	NEGB,
	NEGW,
	NEGL,
	NEGQ,
	NOTB,
	NOTW,
	NOTL,
	NOTQ,
	XORB,
	XORW,
	XORL,
	XORQ,
	ORB,
	ORW,
	ORL,
	ORQ,
	ANDB,
	ANDW,
	ANDL,
	ANDQ,
	CMPB,
	CMPW,
	CMPL,
	CMPQ,
	TESTB,
	TESTW,
	TESTL,
	TESTQ,
	PUSH,
	PUSH_DIRECT, //Bypass live_ranges entirely
	POP,
	POP_DIRECT, //Bypass live_ranges entirely
	SETE, //Set if equal
	SETNE, //Set if not equal
	SETGE, //Set >= signed
	SETLE, //Set <= signed
	SETL, //Set < signed
	SETG, //Set > signed
	SETAE, //Set >= unsigned
	SETA, //Set > unsigned
	SETBE, //Set <= unsigned
	SETB, //Set < unsigned
} instruction_type_t;


#endif /* X86_ASSEMBLY_INSTRUCTION_H */
