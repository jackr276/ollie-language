/**
 * Author: Jack Robbins
 * This file implements the APIs as described in the header file of the same name
 */

#include "variable_mapping.h"


/**
 * Allocate a variable map with the default size
 */
variable_map_t variable_map_alloc(){
	variable_map_t map;

	//Allocate a buffer with the default size to start
	map.max_index = VARIABLE_MAPPING_DEFAULT_SIZE;
	map.mappings = calloc(sizeof(variable_mapping_t), map.max_index);

	map.current_index = 0;
	return map;
}


/**
 * Deallocate a given variable map
 */
void variable_map_dealloc(variable_map_t* map){
	//Destroy the mappings
	free(map->mappings);

	//0 these out to be safe
	map->current_index = 0;
	map->max_index = 0;
}
