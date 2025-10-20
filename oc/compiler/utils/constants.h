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

#endif /* OLLIE_COMPILER_CONSTANTS_H */
