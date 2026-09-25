/**
 * Author: Jack Robbins
 * This header file defines the APIs for the variable mapping. The variable mapping
 * is used when we clone instructions from one function to another and need 
 * distinct(both memory distinct and ID distinct) values
 *
 * This header file contains API definitions that are implemented in the appropriate C
 * file but also contains inline definitions for commonly reused, lightweight helpers
 *
 * Something important to note about variable maps is that they are ephemeral, they are creating
 * at the point of every single inlining. Because of this, we do not need to worry about preserving
 * any kind of state with them
 *
 * RESTRICTIONS: we assume that in any one mapping, one unique variable ID may only be mapped once.
 * In other words, we may not map the variable with ID 2 twice in distinct mappings. This would make
 * no sense and would violate our positional encoding
 */

#ifndef VARIABLE_MAPPING_H
#define VARIABLE_MAPPING_H

#include <sys/types.h>
#include "../../symtab/symtab.h"

//Individual variable mappings
typedef struct variable_mapping_t variable_mapping_t;
//The overall map itself
typedef struct variable_map_t variable_map_t;

/**
 * What kind of variables are we mapping? We can
 * have temp-to-temp or symtab-to-symtab
 */
typedef enum {
	MAPPING_TYPE_TEMP_TO_TEMP,
	MAPPING_TYPE_SYMTAB_TO_SYMTAB,
	MAPPING_TYPE_TEMP_TO_SYMTAB,
	MAPPING_TYPE_SYMTAB_TO_TEMP,
} variable_mapping_type_t;


/**
 * The variable mapping itself consists of unions of either
 * temp var ID's or symtab variables. We will decide which one 
 * to draw from based entirely on the mapping type
 */
struct variable_mapping_t {
	union {
		u_int32_t temporary_id;
		symtab_variable_record_t* symtab_variable;
	} source;

	union {
		u_int32_t temporary_id;
		symtab_variable_record_t* symtab_variable;
	} destination;

	//Are we mapping temp-to-temp or symtab-to-symtab
	variable_mapping_type_t mapping_type;

	/**
	 * Unique, atomically increasing mapping ID
	 */
	int32_t mapping_id;
};


/**
 * The overall map holds a dynamically resizing
 * array of mappings that are stored as a contiguous
 * memory chunk(not pointers)
 */
struct variable_map_t {
	variable_mapping_t* mappings;
	int32_t current_index;
	int32_t max_index;
};

//====================================== Non-Inlined Functions ===============================================================
/**
 * Create a new mapping for a temporary variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that is on you
 */
void create_mapping_for_temporary_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id, u_int32_t dest_temp_var_id);

/**
 * Create a new mapping for a symtab variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that is on you
 */
void create_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable, symtab_variable_record_t* destination_variable);

/**
 * Create a new mapping that goes from a temp var to a symtab variable
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that is on you
 */
void create_mapping_for_temp_to_symtab_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id, symtab_variable_record_t* destination_variable);

/**
 * Allocate a variable map with the default size
 */
variable_map_t variable_map_alloc();

/**
 * Deallocate a given variable map
 */
void variable_map_dealloc(variable_map_t* map);
//====================================== Non-Inlined Functions ===============================================================

//====================================== Inlined Utility Functions ===========================================================
/**
 * Crawl the variable map looking specifically for a temporary variable mapping
 * that has the given source variable ID. We return NULL if none is found
 */
static inline variable_mapping_t* get_mapping_for_temporary_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id){
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
static inline variable_mapping_t* get_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable){
	/**
	 * First try: get the mapping using the variable ID here. If we get a mapping and the source
	 * matches then we are going to skip the linear scan
	 */
	variable_mapping_t* mapping = &(variable_map->mappings[source_variable->mapping_id]);
	if(mapping != NULL && mapping->source.symtab_variable == source_variable){
		return mapping;
	}

	/**
	 * Second try: if that didn't work then we'll just do our regular linear scan over
	 * every single mapping in here
	 */
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
//====================================== Inlined Utility Functions ===========================================================
#endif /* VARIABLE_MAPPING_H */
