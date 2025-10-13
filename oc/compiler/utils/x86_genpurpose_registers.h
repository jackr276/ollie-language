/**
 * Author: Jack Robbins
 * 
 * Define all of our general use registers
*/

//Include guards
#ifndef X86_GENPUPROSE_REGISTERS_H
#define X86_GENPUPROSE_REGISTERS_H

/**
 * Define the standard x86-64 register table
 */
typedef enum{
	NO_REG = 0, //Default is that there's no register used
	RAX,
	RCX,
	RDX,
	RSI,
	RDI,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15, 
	RBX,
	RBP, //base pointer
	//ALL general purpose registers come first(items 1-15)
	RSP, //Stack pointer
	RIP, //Instruction pointer
} general_purpose_register_t;

#endif /* X86_GENPUPROSE_REGISTERS_H */
