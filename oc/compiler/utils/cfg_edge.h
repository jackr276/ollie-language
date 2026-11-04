/**
 * Author: Jack Robbins
 * This file contains the definition for the cfg_edge_t struct that is uses inside of a basic block's 
 * predecessor and successor sets. We require this to be a struct because the Ollie compiler has both
 * real edges and virtual(analysis only) edges. We maintain a tag inside of this struct to distinguish
 * between the two
 */

#include "basic_block.h"


typedef struct cfg_edge_t cfg_edge_t;
struct cfg_edge_t {

};
