/**
 * Author: Jack Robbins
 * This header contains a struct that represents the RFLAGS register. We will use
 * it inside of our simulator to simulate branch jumps based on known conditionals
 */

#include <sys/types.h>

#define CARRY_FLAG_BIT 0
#define PARITY_FLAG_BIT 2
#define AUXILIARY_CARRY_FLAG_BIT 4
#define ZERO_FLAG_BIT 6
#define SIGN_FLAG_BIT 7
#define OVERFLOW_FLAG_BIT 11

/**
 * Define these flag values for when we are trying to set
 * flags. We can only set them on our off
 */
typedef enum {
	FLAG_VALUE_UNSET = 0,
	FLAG_VALUE_SET = 1,
} flag_value_t;


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


/**
 * Retrieve the bit that is in the carry flag
 */
static inline u_int8_t get_carry_flag(RFLAGS_t* flags){
	u_int8_t bit_mask = 0x01;
	return (flags->flag_register >> CARRY_FLAG_BIT) & bit_mask;
}


/**
 * Retrieve the bit that is in the parity flag
 */
static inline u_int8_t get_parity_flag(RFLAGS_t* flags){
	u_int8_t bit_mask = 0x01;
	return (flags->flag_register >> PARITY_FLAG_BIT) & bit_mask;
}
