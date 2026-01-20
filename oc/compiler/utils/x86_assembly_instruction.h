/**
 * Author: Jack Robbins
 * This header file contains the definitions for all x86-64 instructions that OC
 * produces
*/

#ifndef X86_ASSEMBLY_INSTRUCTION_H
#define X86_ASSEMBLY_INSTRUCTION_H

/**
 * What memory access type do we have for a given
 * instruction? By default it's NO_MEMORY_ACCESS(0), 
 * and the other options WRITE_TO_MEMORY and READ_FROM_MEMORY
 * represent reads & writes respectively
 */
typedef enum {
	NO_MEMORY_ACCESS = 0,
	WRITE_TO_MEMORY,
	READ_FROM_MEMORY
} memory_access_type_t;


/**
 * What kind of memory addressing mode do we have?
 */
typedef enum{
	ADDRESS_CALCULATION_MODE_NONE = 0, //default is always none
	ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE, //(%rax) - only the deref depending on how much indirection
	ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST, //(%rax) - only the deref depending on how much indirection
	ADDRESS_CALCULATION_MODE_OFFSET_ONLY, // 4(%rax)
	ADDRESS_CALCULATION_MODE_REGISTERS_ONLY, // (%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET, // 4(%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE, // (%rax, %rcx, 8)
	ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE, // 4(%rax, %rcx, 8)
	ADDRESS_CALCULATION_MODE_INDEX_AND_SCALE, // (, %rcx, 8)
	ADDRESS_CALCULATION_MODE_INDEX_OFFSET_AND_SCALE, // 4(, %rcx, 8)
	ADDRESS_CALCULATION_MODE_RIP_RELATIVE, //RIP relative addresing like: <val>(%rip)
	ADDRESS_CALCULATION_MODE_RIP_RELATIVE_WITH_OFFSET //RIP relative addresing like: <offset> + <val>(%rip)
} address_calculation_mode_t;


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
	MOVSBW, //Move signed byte to word
	MOVSBL, //Move signed byte to long 
	MOVSBQ, //Move signed byte to quad 
	MOVSWL, //Move signed word to long
	MOVSWQ, //Move signed word to quad 
	MOVSLQ, //Move signed long to quad 
	MOVZBW, //Move unsigned byte to word
	MOVZBL, //Move unsigned byte to long 
	MOVZBQ, //Move unsigned byte to quad 
	MOVZWL, //Move unsigned word to long
	MOVZWQ, //Move unsigned word to quad 
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
	PUSH_DIRECT_GP,  //Bypass live_ranges entirely
	PUSH_DIRECT_SSE, //Bypass live ranges entirely
	POP,
	POP_DIRECT_GP, 	 //Bypass live_ranges entirely
	POP_DIRECT_SSE,	 //Bypass live_ranges entirely
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
	// ============= Begin specialized floating point instructions ==============
	ADDSD, //Add scalar f64
	SUBSD, //Subtract scalar f64
	ADDSS, //Add scalar f32
	SUBSS, //Subtract scalar f32
	DIVSS, //Divide scalar f32
	DIVSD, //Divide scalar f64
	MULSS, //Multiply scalar f32
	MULSD, //Multiply scalar f64
	MOVSS, //Move f32 -> f32
	MOVSD, //Move f64 -> f64
	COMISS, //Ordered compare of f32(throws FP exception)
	COMISD, //Ordered compare of f64(throw FP exception)
	UCOMISS, //Unordered compare of f32
	UCOMISD, //Unordered compare of f64
	MOVAPS, //Move aligned packed f32 -> used if we need to clear out the whole thing
	MOVAPD, //Move aligned packed f64 -> used if we need to clear out the whole thing
	CVTSS2SD, //Convert scalar f32 to scalar f64
	CVTSD2SS, //Convert scalar f64 to scalar f32
	CVTTSD2SIL, //Convert scalar f64 to i32 with truncation
	CVTTSD2SIQ, //Convert scalar f64 to i64 with truncation
	CVTTSS2SIL, //Convert scalar f32 to i32 with truncation
	CVTTSS2SIQ, //Convert scalar f32 to i64 with truncation
	CVTSI2SSL, //Convert scalar i32 to f32	
	CVTSI2SSQ, //Convert scalar i64 to f32
	CVTSI2SDL, //Convert scalar i32 to f64	
	CVTSI2SDQ, //Convert scalar i64 to f64
	PXOR, //Packed logical exclusive or
	PAND, //Packed logical and
	PANDN, //Packed logical and not
	POR, //Packed logical or
} instruction_type_t;


#endif /* X86_ASSEMBLY_INSTRUCTION_H */
