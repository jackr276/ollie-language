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

//The number of colors that we have for general use registers
#define K_COLORS_GEN_USE 15

//A load and a store generate 2 instructions when we load
//from the stack
#define LOAD_AND_STORE_COST 2

//A large prime for hashing
#define LARGE_PRIME 611593

//The default is 20 -- this can always be reupped
#define DYNAMIC_ARRAY_DEFAULT_SIZE 20 

//Default length of the string is 60 characters
#define DEFAULT_DYNAMIC_STRING_LENGTH 60

#endif /* OLLIE_COMPILER_CONSTANTS_H */
