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
 * Crawl the variable map looking specifically for a temporary variable mapping
 * that has the given source variable ID. We return NULL if none is found
 */
variable_mapping_t* get_mapping_for_temporary_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id){
	for(int32_t i = 0; i < variable_map->current_index; i++){
		/**
		 * We only care to look for temp var mappings here - if it's not
		 * that then leave
		 */
		if(variable_map->mappings[i].mapping_type != MAPPING_TYPE_TEMP){
			continue;
		}

		/**
		 * We have a hit - return the address of this mapping to avoid copying
		 */
		if(variable_map->mappings[i].source.temporary_id == source_temp_var_id){
			return &(variable_map->mappings[i]);
		}
	}

	//If we made it here then we found nothing so bail out
	return NULL;
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
