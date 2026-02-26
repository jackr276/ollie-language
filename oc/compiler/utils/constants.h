/**
 * Author: Jack Robbins
 * This file contains the definitions for all of the constants used
 * by the ollie compiler
*/

//Include guards
#ifndef OLLIE_COMPILER_CONSTANTS_H
#define OLLIE_COMPILER_CONSTANTS_H

#define TRUE 1
#define FALSE 0

//Can also have success/failure too
#define SUCCESS 1
#define FAILURE 0

//For loops, we estimate that they'll execute 10 times each
#define LOOP_ESTIMATED_COST 10

//The max switch/case range is 1024
#define MAX_SWITCH_RANGE 1024

//All error sizes are 2000
#define ERROR_SIZE 2000

//The maximum number of per-class register passed parameters
#define MAX_PER_CLASS_REGISTER_PASSED_PARAMS 6

//The size that it takes for a parameter to be callee-saved
#define CALLEE_SAVED_REGISTER_STACK_SIZE_BYTES 8

//The number of colors that we have for general use registers
#define K_COLORS_GEN_USE 15

//The number of colors that we have for XMM floating point registers
#define K_COLORS_SSE 16

//A load and a store generate 2 instructions when we load
//from the stack
#define LOAD_COST 2

//Storing also generates 2 instructions
#define STORE_COST 2

//A large prime for hashing
#define LARGE_PRIME 611593

//Default size for a token array is 5. This is smaller because we don't likely
//have tons of tokens in our macro
#define TOKEN_ARRAY_DEFAULT_SIZE 5

//The default is 20 -- this can always be reupped
#define DYNAMIC_ARRAY_DEFAULT_SIZE 20 

//Default length of the string is 60 characters
#define DEFAULT_DYNAMIC_STRING_LENGTH 60

//For the dynamic arrays/sets
#define NOT_FOUND -1

//Definitions for cycle counts on our known instructions
//Load cycle count is 1, we perform the real estimation
//later on in the scheduler
#define LOAD_CYCLE_COUNT 1
#define STORE_CYCLE_COUNT 2
#define UNSIGNED_INT_MULTIPLY_CYCLE_COUNT 3
#define SIGNED_INT_MULTIPLY_CYCLE_COUNT 3
#define SIGNED_INT_DIVIDE_CYCLE_COUNT 30
#define UNSIGNED_INT_DIVIDE_CYCLE_COUNT 30
#define DEFAULT_CYCLE_COUNT 1

#endif /* OLLIE_COMPILER_CONSTANTS_H */
