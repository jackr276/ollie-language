/**
 * Author: Jack Robbins
 * This header file defines the APIs for the variable mapping. The variable mapping
 * is used when we clone instructions from one function to another and need 
 * distinct(both memory distinct and ID distinct) values
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
	MAPPING_TYPE_TEMP,
	MAPPING_TYPE_SYMTAB,
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


/**
 * Crawl the variable map looking specifically for a temporary variable mapping
 * that has the given source variable ID. We return NULL if none is found
 */
variable_mapping_t* get_mapping_for_temporary_variable(variable_map_t* variable_map, u_int32_t source_temp_var_id);

/**
 * Crawl the variable map looking specifically for a symtab variable mapping
 * that has the given source symtab variable. We return NULL if none is found
 */
variable_mapping_t* get_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable);

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
 * Allocate a variable map with the default size
 */
variable_map_t variable_map_alloc();


/**
 * Deallocate a given variable map
 */
void variable_map_dealloc(variable_map_t* map);

#endif /* VARIABLE_MAPPING_H */
