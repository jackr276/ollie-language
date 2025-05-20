/**
 * Author: Jack Robbins
 *
 * This contains the APIs for the stack data area. The stack data area is designed to be a "living" data
 * structure that we can manipulate as a program goes through optimization and register selection. We can
 * add/remove values as needed
 *
 * The stack data area itself will be organized as a self-sorting list where the largest values are
 * placed at the very bottom. This allows us to save overall space with our alignment
*/

//Header guards
#ifndef STACK_DATA_AREA_H
#define STACK_DATA_AREA_H

#include <sys/types.h>

//An overall structure that holds our stack data area
typedef struct stack_data_area_t stack_data_area_t;
//Each node represents the allocation of a certain variable of a certain size
typedef struct stack_data_area_node_t stack_data_area_node_t;


struct stack_data_area_t{
	//The head node of the data area.
	//This will always be the highest(i.e. *lowest* offset) node 
	stack_data_area_node_t* highest;
	//The total size of the data area
	u_int32_t total_size;
};


struct stack_data_area_node_t{

};


#endif /* STACK_DATA_AREA_H */
