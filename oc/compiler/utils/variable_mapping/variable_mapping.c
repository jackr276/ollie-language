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
		//Get a pointer to the mapping
		variable_mapping_t* mapping = &(variable_map->mappings[i]);

		/**
		 * We only care to look for temp var mappings here - if it's not
		 * that then skip
		 */
		if(mapping->mapping_type != MAPPING_TYPE_TEMP_TO_SYMTAB && mapping->mapping_type != MAPPING_TYPE_TEMP_TO_TEMP){
			continue;
		}

		/**
		 * We have a hit - return the address of this mapping to avoid copying
		 */
		if(mapping->source.temporary_id == source_temp_var_id){
			return mapping;
		}
	}

	//If we made it here then we found nothing so bail out
	return NULL;
}


/**
 * Crawl the variable map looking specifically for a symtab variable mapping
 * that has the given source symtab variable. We return NULL if none is found
 */
variable_mapping_t* get_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable){
	/**
	 * First try: get the mapping using the variable ID here. If we get a mapping and the source
	 * matches then we are going to skip the linear scan
	 */


	if(variable_map->mappings[source_variable->mapping_id].source.symtab_variable == source_variable){
		return ;
	}


	for(int32_t i = 0; i < variable_map->current_index; i++){
		//Get a pointer to the mapping
		variable_mapping_t* mapping = &(variable_map->mappings[i]);

		/**
		 * We only care to look for symtab mappings here - if it's not
		 * that then skip
		 */
		if(mapping->mapping_type != MAPPING_TYPE_SYMTAB_TO_SYMTAB && mapping->mapping_type != MAPPING_TYPE_SYMTAB_TO_TEMP){
			continue;
		}

		/**
		 * We have a hit - return the address of this mapping to avoid copying
		 */
		if(mapping->source.symtab_variable == source_variable){
			return mapping;
		}
	}

	//If we made it here then we found nothing so bail out
	return NULL;

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

	//The mapping ID is the index where it exists
	mapping->mapping_id = variable_map->current_index;

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

	//The mapping ID is the index where it exists
	mapping->mapping_id = variable_map->current_index;

	//Store on the symtab variable the mapping that we correspond to
	source_variable->mapping_id = mapping->mapping_id;

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

	//The mapping ID is the index where it exists
	mapping->mapping_id = variable_map->current_index;

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
