/**
 * Author: Jack Robbins
 * This header contains a struct that represents the RFLAGS register. We will use
 * it inside of our simulator to simulate branch jumps based on known conditionals.
 * This header file defines the RFLAGS_t struct and provides utilities for getting
 * and setting the bits in interest
 */

#include <sys/types.h>

#define CARRY_FLAG_BIT 0
#define PARITY_FLAG_BIT 2
#define AUXILIARY_CARRY_FLAG_BIT 4
#define ZERO_FLAG_BIT 6
#define SIGN_FLAG_BIT 7
#define OVERFLOW_FLAG_BIT 11

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


/**
 * Retrieve the bit that represents the carry flag
 */
static inline u_int8_t get_carry_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	return (flags->flag_register >> CARRY_FLAG_BIT) & bit_mask;
}


/**
 * Set the bit that represents the carry flag
 *
 * NOTE: the set function always sets the value to 1
 */
static inline void set_carry_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	bit_mask <<= CARRY_FLAG_BIT;
	flags->flag_register |= bit_mask;
}


/**
 * Retrieve the bit that represents the parity flag
 */
static inline u_int8_t get_parity_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	return (flags->flag_register >> PARITY_FLAG_BIT) & bit_mask;
}


/**
 * Set the bit that represents the parity flag
 *
 * NOTE: the set function always sets the value to 1
 */
static inline void set_parity_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	bit_mask <<= PARITY_FLAG_BIT;
	flags->flag_register |= bit_mask;
}


/**
 * Retrieve the bit that represents the auxiliary carry flag
 */
static inline u_int8_t get_auxiliary_carry_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	return (flags->flag_register >> AUXILIARY_CARRY_FLAG_BIT) & bit_mask;
}


/**
 * Set the bit that represents the auxiliary flag
 *
 * NOTE: the set function always sets the value to 1
 */
static inline void set_auxiliary_carry_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	bit_mask <<= AUXILIARY_CARRY_FLAG_BIT;
	flags->flag_register |= bit_mask;
}


/**
 * Retrieve the bit that represents the zero flag
 */
static inline u_int8_t get_zero_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	return (flags->flag_register >> ZERO_FLAG_BIT) & bit_mask;
}


/**
 * Set the bit that represents the zero flag
 *
 * NOTE: the set function always sets the value to 1
 */
static inline void set_zero_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	bit_mask <<= ZERO_FLAG_BIT;
	flags->flag_register |= bit_mask;
}


/**
 * Retrieve the bit that represents the sign flag
 */
static inline u_int8_t get_sign_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	return (flags->flag_register >> SIGN_FLAG_BIT) & bit_mask;
}


/**
 * Set the bit that represents the sign flag
 *
 * NOTE: the set function always sets the value to 1
 */
static inline void set_sign_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	bit_mask <<= SIGN_FLAG_BIT;
	flags->flag_register |= bit_mask;
}


/**
 * Retrieve the bit that represents the overflow flag
 */
static inline u_int8_t get_overflow_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	return (flags->flag_register >> OVERFLOW_FLAG_BIT) & bit_mask;
}


/**
 * Set the bit that represents the overflow flag
 *
 * NOTE: the set function always sets the value to 1
 */
static inline void set_overflow_flag(RFLAGS_t* flags){
	u_int64_t bit_mask = 0x01;
	bit_mask <<= OVERFLOW_FLAG_BIT;
	flags->flag_register |= bit_mask;
}
