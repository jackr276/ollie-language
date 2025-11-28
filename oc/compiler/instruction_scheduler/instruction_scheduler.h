/**
 * Author: Jack Robbins
 *
 * This header defines the APIs for the instruction scheduler submodule
*/

//Include guards
#ifndef INSTRUCTION_SCHEDULER_H
#define INSTRUCTION_SCHEDULER_H
#include "../cfg/cfg.h"

/**
 * Expose the root level API to the front end. Obviously
 * there is much more that goes on behind the scenes here with the sceduler
 */
cfg_t* schedule_all_instructions(cfg_t* cfg);

#endif /* INSTRUCTION_SCHEDULER_H */
