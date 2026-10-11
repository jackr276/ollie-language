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
 *
 * Architecture: the variable map is made of a paged sparse array where each page has a size of 128
 * records. Most functions that are inlined will not require 128 variables so this size is chosen
 * to encapsulate most functions in one allocation
 */

#ifndef VARIABLE_MAPPING_H
#define VARIABLE_MAPPING_H

#include <sys/types.h>
#include "../../symtab/symtab.h"
#include "../constants.h"

//We hold 128 mappings per page
#define VARIABLE_MAPPING_PAGE_SIZE 128

//By default we hold 1024 pages(8kb array of pointers)
#define DEFAULT_PAGE_COUNT 1024

//Individual variable mappings
typedef struct variable_mapping_t variable_mapping_t;
//A "page" of variable maps
typedef struct variable_mapping_page_t variable_mapping_page_t;
//The overall map itself
typedef struct variable_map_t variable_map_t;

/**
 * What kind of variables are we mapping? We can
 * have temp-to-temp or symtab-to-symtab
 */
typedef enum {
	MAPPING_TYPE_NOT_IMPLEMENTED = 0, //sentinel value 
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
};


/**
 * A variable mapping "page" consists of 128 variable
 * mappings stored in a flat array. Each mapping is
 * logically keyed by the source variable's unique
 * "variable_id"
 */
struct variable_mapping_page_t {
	variable_mapping_t mappings[VARIABLE_MAPPING_PAGE_SIZE];
};


/**
 * The variable map itself consists of "pages" of mappings that are
 * contiguous 128 mapping chunks. The overall mapping_count tells
 * us how many mapping pages there are. We will try to allocate a large
 * number of pages to start off with and should hopefully only need to
 * resize these rarely
 */
struct variable_map_t {
	variable_mapping_page_t** page_table;
	int32_t page_count;
};


//====================================== Non-Inlined Functions ===============================================================
/**
 * Allocate the variable map. Remember that our intention is to only have one variable map per compilation
 * run, so this is intentionally a relatively large upfront allocation(8kb) in the hopes of rarely having
 * to resize
 */
variable_map_t variable_map_alloc();


/**
 * Deallocate a given variable map
 */
void variable_map_dealloc(variable_map_t* map);

//TODO
//
//
//


/**
 * Create a new mapping for a temporary variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_temporary_variable(variable_map_t* variable_map, int32_t source_temp_var_id, int32_t dest_temp_var_id);

/**
 * Create a new mapping for a symtab variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable, symtab_variable_record_t* destination_variable);

/**
 * Create a new mapping that goes from a temp var to a symtab variable
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_temp_to_symtab_variable(variable_map_t* variable_map, int32_t source_temp_var_id, symtab_variable_record_t* destination_variable);

//====================================== Non-Inlined Functions ===============================================================
//====================================== Inlined Utility Functions ===========================================================
/**
 * Get the mapping from the list if it exists. If it does not then we return NULL
 *
 * To get the indices we will do:
 * 		page_number = source_id / PAGE_SIZE
 * 		page_offset = source_id % PAGE_SIZE
 *
 * If the page is NULL then return NULL. Otherwise get page[offset]
 */
static inline variable_mapping_t* get_mapping_for_temporary_variable(variable_map_t* variable_map, int32_t source_temp_var_id){
	//Compute the page number and offset
	int32_t page_number = source_temp_var_id / VARIABLE_MAPPING_PAGE_SIZE;
	int32_t page_offset = source_temp_var_id % VARIABLE_MAPPING_PAGE_SIZE;

	//Fail case 1 - we've exceeded the page count
	if(page_number > variable_map->page_count){
		return NULL;
	}

	//Fail case 2 - the page was never allocated
	variable_mapping_page_t* page = variable_map->page_table[page_number];
	if(page == NULL){
		return NULL;
	}

	/**
	 * If we get here and we see it's not unimplemented, then we get the mapping,
	 * otherwise we return NULL
	 */
	variable_mapping_t* mapping = &(page->mappings[page_offset]);
	return mapping->mapping_type != MAPPING_TYPE_NOT_IMPLEMENTED ? mapping : NULL;
}


/**
 * Crawl the variable map looking specifically for a symtab variable mapping
 * that has the given source symtab variable. We return NULL if none is found
 *
 * NOTE: for symtab variables we always use the "variable_id" and we ignore the memory
 * address variable ID
 */
static inline variable_mapping_t* get_mapping_for_symtab_variable(variable_map_t* variable_map, symtab_variable_record_t* source_variable){
	/**
	 * For the source variable ID, we will default to use the regular "variable_id" if it's not unset.
	 * However, if it does not exist, we will fallback to the memory address variable id. Some variables
	 * exist only as memory addresses which is why we need to do this
	 */
	int32_t source_var_id;
	if(source_variable->associated_three_addr_var_ids.variable_id != NEVER_SET){
		source_var_id = source_variable->associated_three_addr_var_ids.variable_id;
	} else {
		source_var_id = source_variable->associated_three_addr_var_ids.memory_address_variable_id;
	}

	//Get the mapping using this source var ID with the adjustment
	variable_mapping_t* mapping = &(variable_map->mappings[source_var_id - variable_map->index_adjustment]);

	/**
	 * If we get here and we see it's not unimplemented, then we get the mapping,
	 * otherwise we return NULL
	 */
	if(mapping->mapping_type != MAPPING_TYPE_NOT_IMPLEMENTED){
		return mapping;
	} else {
		return NULL;
	}
}
//====================================== Inlined Utility Functions ===========================================================
#endif /* VARIABLE_MAPPING_H */
