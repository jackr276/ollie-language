/**
 * Author: Jack Robbins
 * This file implements the APIs as described in the header file of the same name
 */

#include "variable_mapping.h"

/**
 * Allocate a variable map designed specifically for a given function. Remember that variable
 * maps are specific to a given function that we're inlining. They may not be reused and
 * must be rebuilt upon every single inline request
 */
variable_map_t variable_map_alloc(symtab_function_record_t* mapped_function){
	//Stack allocate the map
	variable_map_t map;

	/**
	 * IMPORTANT - we will maintain a so-called "index-adjustment"
	 * so that the smallest variable ID inside of this function will
	 * map to index 0 when we add/retrieve
	 */
	map.index_adjustment = mapped_function->min_variable_id;

	/**
	 * Say our function has the lowest variable at ID 15 and the highest variable
	 * ID at 57. We will need to store 43 values(we need to store 15 too), so 
	 * we're storing 57 - 15 + 1
	 */
	map.mapping_count = mapped_function->max_variable_id - mapped_function->min_variable_id + 1;

	//Allcoate based on our size, and clear the current index out
	map.mappings = calloc(sizeof(variable_map_t), map.mapping_count);

	//Return a copy
	return map;
}


/**
 * Perform the dynamic resize for the variable map if we determine that it's needed
 */
static inline void dynamically_resize_if_needed(variable_map_t* variable_map){
	if(variable_map->current_index == variable_map->max_index){
		//Always double it to be safe
		variable_map->max_index *= 2;

		//Reallocate to a larger buffer
		variable_map->mappings = realloc(variable_map->mappings, sizeof(variable_mapping_t) * variable_map->max_index);
	}
}


/**
 * Create a new mapping for a temporary variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that is on you
 */
void create_mapping_for_temporary_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id, u_int32_t dest_temp_var_id){
	//Perform the resize if need be
	dynamically_resize_if_needed(variable_map);

	//Grab a reference just to make this neater
	variable_mapping_t* mapping = &(variable_map->mappings[variable_map->current_index]);

	//This is a temp mapping
	mapping->mapping_type = MAPPING_TYPE_TEMP_TO_TEMP;

	//Store the source and dest
	mapping->source.temporary_id = source_temp_var_id;
	mapping->destination.temporary_id = dest_temp_var_id;

	//Bump this up for the next go around
	(variable_map->current_index)++;
}


/**
 * Create a new mapping for a symtab variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that is on you
 */
void create_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable, symtab_variable_record_t* destination_variable){
	//Perform the resize if needed
	dynamically_resize_if_needed(variable_map);

	//Grab a reference to the region to make this easier
	variable_mapping_t* mapping = &(variable_map->mappings[variable_map->current_index]);

	//This is a symtab mapping
	mapping->mapping_type = MAPPING_TYPE_SYMTAB_TO_SYMTAB;

	//Store the source and dest
	mapping->source.symtab_variable = source_variable;
	mapping->destination.symtab_variable = destination_variable;

	//Bump this up for the next go around
	(variable_map->current_index)++;
}


/**
 * Create a new mapping that goes from a temp var to a symtab variable
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that is on you
 */
void create_mapping_for_temp_to_symtab_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id, symtab_variable_record_t* destination_variable){
	//Perform the resize if needed
	dynamically_resize_if_needed(variable_map);

	//Grab a reference to the region to make this easier
	variable_mapping_t* mapping = &(variable_map->mappings[variable_map->current_index]);

	//This is a symtab mapping
	mapping->mapping_type = MAPPING_TYPE_TEMP_TO_SYMTAB;

	//Store the source and dest
	mapping->source.temporary_id = source_temp_var_id;
	mapping->destination.symtab_variable = destination_variable;

	//Bump this up for the next go around
	(variable_map->current_index)++;
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
