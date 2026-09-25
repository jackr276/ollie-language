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

	//Allcoate based on our size
	map.mappings = calloc(sizeof(variable_mapping_t), map.mapping_count);

	//Return a copy
	return map;
}

/**
 * Create a new mapping for a temporary variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_temporary_variable(variable_map_t* variable_map, int32_t source_temp_var_id, int32_t dest_temp_var_id){
	/**
	 * IMPORTANT - adjust the index for the source ID so that we have a 0-indexed array of mappings
	 * by variable ID, regardless of what the actual ID is
	 */
	int32_t adjusted_index = source_temp_var_id - variable_map->index_adjustment;

	//Grab a reference just to make this neater
	variable_mapping_t* mapping = &(variable_map->mappings[adjusted_index]);

	//This is a temp mapping
	mapping->mapping_type = MAPPING_TYPE_TEMP_TO_TEMP;

	//Store the source and dest
	mapping->source.temporary_id = source_temp_var_id;
	mapping->destination.temporary_id = dest_temp_var_id;
}


/**
 * Create a new mapping for a symtab variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable, symtab_variable_record_t* destination_variable){
	//TODO DOC
	int32_t source_var_id;
	if(source_variable->associated_three_addr_var_ids.variable_id != NEVER_SET){
		source_var_id = source_variable->associated_three_addr_var_ids.variable_id;
	} else {
		source_var_id = source_variable->associated_three_addr_var_ids.memory_address_variable_id;
	}

	if(source_var_id == NEVER_SET){
		printf("FATAL THIS WAS NEVER SET HOW IS THAT POSSIBLE\n\n");
	}

	/**
	 * IMPORTANT - adjust the index for the source ID so that we have a 0-indexed array of mappings
	 * by variable ID, regardless of what the actual ID is
	 */
	int32_t adjusted_index = source_var_id - variable_map->index_adjustment; 

	//Grab a reference to the region to make this easier
	variable_mapping_t* mapping = &(variable_map->mappings[adjusted_index]);

	//This is a symtab mapping
	mapping->mapping_type = MAPPING_TYPE_SYMTAB_TO_SYMTAB;

	//Store the source and dest
	mapping->source.symtab_variable = source_variable;
	mapping->destination.symtab_variable = destination_variable;
}


/**
 * Create a new mapping that goes from a temp var to a symtab variable
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_temp_to_symtab_variable(variable_map_t* variable_map, int32_t source_temp_var_id, symtab_variable_record_t* destination_variable){
	/**
	 * IMPORTANT - adjust the index for the source ID so that we have a 0-indexed array of mappings
	 * by variable ID, regardless of what the actual ID is
	 */
	int32_t adjusted_index = source_temp_var_id - variable_map->index_adjustment;

	//Grab a reference to the region to make this easier
	variable_mapping_t* mapping = &(variable_map->mappings[adjusted_index]);

	//This is a symtab mapping
	mapping->mapping_type = MAPPING_TYPE_TEMP_TO_SYMTAB;

	//Store the source and dest
	mapping->source.temporary_id = source_temp_var_id;
	mapping->destination.symtab_variable = destination_variable;
}


/**
 * Deallocate a given variable map
 */
void variable_map_dealloc(variable_map_t* map){
	//Destroy the mappings
	free(map->mappings);
	map->mapping_count = 0;
	map->index_adjustment = 0;
}
