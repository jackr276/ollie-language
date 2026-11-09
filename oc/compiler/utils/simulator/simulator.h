/**
 * Author: Jack Robbins
 * This header file defines the APIs used for the OIR simulator. The OIR simulator allows us
 * to simulate the execution of chains of instructoins. This is particularly useful when it comes
 * to branch reduction when we're able to simplify branching statements
 */

#ifndef OIR_SIMULATOR_H
#define OIR_SIMULATOR_H

#include "../x86_cpu_flags.h"
#include "../basic_block/basic_block.h"
#include <sys/types.h>

/**
 * A simluated execution returns the flag register after execution
 * and a boolean denoting whether a simulation was even possible. If
 * the simulation was not possible, the flags register is junk
 */
typedef struct simulated_execution_result_t simulated_execution_result_t;
struct simulated_execution_result_t{
	RFLAGS_t flag_register;
	u_int8_t simulation_possible;
};

/**
 * Simulate the execution of the given instruction. The simulation will not return the actual value result, instead
 * we will return a struct that contains a boolean noting whether we even could simulate this, and a flag register
 * that represents the state of RFLAGS after this instruction executes
 *
 * There is no guarantee that an instruction passed into this simulator can be simulated
 */
simulated_execution_result_t simulate_instruction_execution(basic_block_t* block, instruction_t* instruction);

#endif /* OIR_SIMULATOR_H */
