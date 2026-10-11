/**
 * Author: Jack Robbins
 * This file implements the APIs as described in the header file of the same name
 */

#include "variable_mapping.h"
#include <string.h>


/**
 * Allocate the variable map. Remember that our intention is to only have one variable map per compilation
 * run, so this is intentionally a relatively large upfront allocation(8kb) in the hopes of rarely having
 * to resize
 */
variable_map_t variable_map_alloc(){
	variable_map_t map;

	/**
	 * Dynamically allocate the page table as an array of pointers
	 * to pages themselves
	 */
	map.page_count = DEFAULT_PAGE_COUNT;
	map.page_table = calloc(sizeof(variable_mapping_page_t*), DEFAULT_PAGE_COUNT);

	return map;
}


/**
 * Get or create the page for the given index. The index represents a source variable
 * ID. In the event that the page table itself does not have enough space to hold the
 * new proposed page, we'll also reallocate the page table
 */
static inline variable_mapping_page_t* get_or_create_page_for_index(variable_map_t* variable_map, int32_t index){
	//Get the page index that we are looking for
	int32_t page_index = index / VARIABLE_MAPPING_PAGE_SIZE;

	/**
	 * First option: we do not currently have a sufficient number of pages
	 * to hold this index. If this is the case we will need to first reallocate
	 * the entire page table so that we have physical space to hold this
	 */
	if(page_index >= variable_map->page_count){
		//Hang onto this for when we 0 out
		int32_t previous_page_count = variable_map->page_count;

		/**
		 * Double the page index and reallocate. Since we rely on unallocated
		 * pages being NULL, we will need to go through and set all of the
		 * new space to be 0
		 */
		variable_map->page_count = page_index * 2;
		variable_map->page_table = realloc(variable_map->page_table, sizeof(variable_mapping_page_t*) * variable_map->page_count);

		//Set everything after the prior page table that we've allocated to be 0
		memset(variable_map->page_table + previous_page_count, 0, sizeof(variable_mapping_page_t*) * (variable_map->page_count - previous_page_count));
	}

	/**
	 * If we get here, we know that we have enough space to hold the page. However, it may not have been created
	 * yet, so now is the time where we'll allocate it if we need to
	 */
	if(variable_map->page_table[page_index] == NULL){
		variable_map->page_table[page_index] = calloc(1, sizeof(variable_mapping_page_t));
	}

	//Give back the pointer to it
	return variable_map->page_table[page_index];
}


/**
 * Create a new mapping for a temporary variable that goes from the source to the destination
 *
 * NOTE: this function will not do duplicate checking. If you mistakenly make a duplicate mapping
 * that overwrites a previous mapping, that is on you
 */
void create_mapping_for_temporary_variable(variable_map_t* variable_map, int32_t source_temp_var_id, int32_t dest_temp_var_id){
	/**
	 * First let the helper determine the correct page to add this mapping into
	 */
	variable_mapping_page_t* page = get_or_create_page_for_index(variable_map, source_temp_var_id);

	/**
	 * Now that we have a page, offset into the page to get a pointer to the mapping that we're
	 * after
	 */
	int32_t page_offset = source_temp_var_id % VARIABLE_MAPPING_PAGE_SIZE;
	variable_mapping_t* mapping = &(page->mappings[page_offset]);

	//Populate the mapping with everything that we need
	mapping->mapping_type = MAPPING_TYPE_TEMP_TO_TEMP;
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

	/**
	 * First let the helper determine the correct page to add this mapping into
	 */
	variable_mapping_page_t* page = get_or_create_page_for_index(variable_map, source_var_id);

	/**
	 * Now that we have the page use the page offset to get a pointer to hte
	 * mapping that we're after
	 */
	int32_t page_offset = source_var_id % VARIABLE_MAPPING_PAGE_SIZE;
	variable_mapping_t* mapping = &(page->mappings[page_offset]);

	//Populate the mapping with what's needed
	mapping->mapping_type = MAPPING_TYPE_SYMTAB_TO_SYMTAB;
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
	 * First let the helper determine the correct page to add this mapping into
	 */
	variable_mapping_page_t* page = get_or_create_page_for_index(variable_map, source_temp_var_id);

	/**
	 * Now that we have the page use the page offset to get a pointer to hte
	 * mapping that we're after
	 */
	int32_t page_offset = source_temp_var_id % VARIABLE_MAPPING_PAGE_SIZE;
	variable_mapping_t* mapping = &(page->mappings[page_offset]);

	//Populate the mapping with what's needed
	mapping->mapping_type = MAPPING_TYPE_TEMP_TO_SYMTAB;
	mapping->source.temporary_id = source_temp_var_id;
	mapping->destination.symtab_variable = destination_variable;
}


/**
 * Deallocate a given variable map by crawling over every entry
 * and deallocating if need be
 */
void variable_map_dealloc(variable_map_t* map){
	//Crawl through every page in the page table and free them
	for(int32_t i = 0; i < map->page_count; i++){
		//Get the page out
		variable_mapping_page_t* page = map->page_table[i];

		if(page != NULL){
			free(page);
		}
	}

	//Free the table and 0 out the count
	free(map->page_table);
	map->page_count = 0;
}
