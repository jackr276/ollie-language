/**
 * Author: Jack Robbins
 * This header contains a struct that represents the RFLAGS register. We will use
 * it inside of our simulator to simulate branch jumps based on known conditionals
 */

#include <sys/types.h>

#define CARRY_FLAG_BIT 0


/**
 * The RFLAGS register itself is just a 64 bit register. The layout
 * that we care about is as follows
 *
 * 	Bit 0 - Carry Flag(CF)
 * 	Bit 2 - Parify Flag(PF)
 * 	Bit 4 - Auxiliary Carry Flag(AF)
 * 	Bit 6 - Zero Flag(ZF)
 * 	Bit 7 - Sign Flag(SF)
 * 	Bit 11 - Overflow Flag(OF)
 */
typedef struct RFLAGS_t RFLAGS_t;
struct RFLAGS_t {
	u_int64_t flag_register;
};


static inline void set_carry_flag(RFLAGS_t* flags){

}

