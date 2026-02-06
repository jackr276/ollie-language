/**
 * Author: Jack Robbins
 *
 * The implementation file for the ollie preprocessor
 *
 * The ollie preproccessor will take two passes over the entire token stream.
 * The first pass will be a consumption pass, where we will read in all of the macros
 * that have been defined. The second pass will be our substitution pass, where all of these macros will be replaced
 * in the file. It should be noted that this will be a destructive process, meaning that we will flag the tokens that
 * were consumed as part of the macro to be ignored by the parser. This avoids any confusion that we may have
*/

#include "preprocessor.h"
#include "../utils/error_management.h"
#include "../utils/constants.h"
#include "../utils/ollie_token_array/ollie_token_array.h"
#include "../symtab/symtab.h"
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>

//What is the name of the file that we are preprocessing
static char* current_file_name;

//Define some holders for failures/warnings
static u_int32_t preprocessor_error_count = 0;
static u_int32_t preprocessor_warning_count = 0;

//For generic error printing
static char info_message[2000];

//The current line number that we're after
static u_int32_t current_line_number;

/**
 * A generic printer for any preprocessor errors that we may encounter
 */
static inline void print_preprocessor_message(error_message_type_t message, char* info, u_int32_t line_number){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[FILE: %s] --> [LINE %d | OLLIE PREPROCESSOR %s]: %s\n", current_file_name, line_number, type[message], info);
}


/**
 * Simple helper that just wraps the token_array_get_pointer_at and takes care of the index bumping
 * for us
 */
static inline lexitem_t* get_token_pointer_and_increment(ollie_token_array_t* array, u_int32_t* index){
	//Extract the token pointer
	lexitem_t* token_pointer = token_array_get_pointer_at(array, *index);

	//Bump the index
	(*index)++;

	//Update the line number
	current_line_number = token_pointer->line_num;

	//Give back the pointer
	return token_pointer;
}


/**
 * Process a macro starting at the begin index
 *
 * NOTE: this function will update the index that is in use here. If this function
 * returns in a success state, the index will be pointing to the token after the ENDMACRO
 * token
 */
static u_int8_t process_macro(ollie_token_stream_t* stream, macro_symtab_t* macro_symtab, u_int32_t* index) {
	//Hang onto this here for convenience
	ollie_token_array_t* token_array = &(stream->token_stream);

	//Let's get the first pointer here
	lexitem_t* lookahead = get_token_pointer_and_increment(token_array, index);

	//This really shouldn't happen because
	//we've already seen the #macro to get here,
	//but we'll catch it just in case
	if(lookahead->tok != MACRO){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "#macro keyword expected before macro declaration", lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//IMPORTANT - flag that this token needs to be ignored by the replacer
	lookahead->ignore = TRUE;

	//Now that we've seen the #macro keyword, we need to see the name
	//of the macro via an identifier
	lookahead = get_token_pointer_and_increment(token_array, index);

	//If we did not see an identifier then we are in bad shape here
	if(lookahead->tok != IDENT){
		sprintf(info_message, "Expected identifier after #macro keyword but got %s", lexitem_to_string(lookahead));
		print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//IMPORTANT - flag that this token needs to be ignored by the replacer
	lookahead->ignore = TRUE;

	//Now that we have a valid identifier, we have all that we need to create the symtab record for this macro
	symtab_macro_record_t* macro_record = create_macro_record(lookahead->lexeme, lookahead->line_num);

	//Grab a pointer to this macro's token array
	ollie_token_array_t* macro_token_array = &(macro_record->tokens);

	//Unbounded loop through the entire macro
	while(TRUE){
		//Refresh the lookahead token
		lookahead = get_token_pointer_and_increment(token_array, index);

		//Based on our token here we'll do a few things
		switch(lookahead->tok){
			//This is bad - there is no such thing as a nested macro and we are already
			//in one
			case MACRO:
				print_preprocessor_message(MESSAGE_TYPE_ERROR, "#macro keyword found inside of a macro definition", lookahead->line_num);
				preprocessor_error_count++;
				return FAILURE;

			//This could be good or bad depending on what we're after
			case ENDMACRO:
				//IMPORTANT - flag that this token needs to be ignored by the replacer
				lookahead->ignore = TRUE;

				//This is invalid, we cannot have a completely 
				//empty macro
				if(macro_token_array->current_index == 0){
					sprintf(info_message, "Ollie macro %s is empty and is therefore invalid", macro_record->name.string);
					print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, macro_record->line_number);
					preprocessor_error_count++;
					return FAILURE;
				}

				//Otherwise this should be fine, so we will go ahead and add this on in
				goto finalize_macro;

			//In theory anything else that we see in here is valid, so we'll
			//just do our bookkeeping and move along
			default:
				//IMPORTANT - flag that this token needs to be ignored by the replacer
				lookahead->ignore = TRUE;

				//Add this into the token array
				token_array_add(macro_token_array, lookahead);
				
				break;
		}
	}

finalize_macro:
	//Get it into the symtab
	insert_macro(macro_symtab, macro_record);

	//Return that we succeeded
	return SUCCESS;
}


/**
 * Put simply, the consumption pass will run through the entire token
 * stream looking for macros. When it finds a macro, it will flag that section
 * of the token stream to be ignored by future passes(in reality this means
 * it will be cut out completely) and will store the macro token snippet
 * inside of a struct for later use. The consumption pass does not have anything
 * to do with macro replacement. This will come after in the replacement
 * pass
 */
static inline u_int8_t macro_consumption_pass(ollie_token_stream_t* stream, macro_symtab_t* macro_symtab){
	//Standard holder for the result of each macro consumption
	u_int8_t result;

	//Keep track of the current array index
	u_int32_t array_index = 0;

	//Loop through the entire structure
	while(array_index < stream->token_stream.current_index){
		//Get a pointer to the token that we are after.
		//
		//IMPORTANT - we want to modify this token in the stream, so a pointer
		//is critical. We *cannot* use a local copy for this
		lexitem_t* token = &(stream->token_stream.internal_array[array_index]);

		//Go based on the kind of token that we have in here
		switch(token->tok){
			//We are seeing the beginning of a macro
			case MACRO:
				//Now we will invoke the helper to parse this entire token
				//stream(until we see the ENDMACRO directive)
				result = process_macro(stream, macro_symtab, &array_index);

				//This indicates some kind of failure. The error message
				//will have already been printed by the processor, so we just
				//pass this along
				if(result == FAILURE){
					return FAILURE;
				}

				break;

			//If we see this, that means we have a floating endmacro in there
			case ENDMACRO:
				print_preprocessor_message(MESSAGE_TYPE_ERROR, "Floating #endmacro directive declared. Are you missing a #macro directive", token->line_num);
				preprocessor_error_count++;
				return FAILURE;

			//We haven't seen a macro, but the array index needs to be bumped
			default:
				array_index++;
				break;
		}
	}

	//If we made it down here, then we can declare success
	return SUCCESS;
}


/**
 * TODO - substitution itself
 */
static u_int8_t perform_macro_substitution(){
	//TODO
}


/**
 * The macro replacement pass will produce an entirely new token stream in which all of our replacements have been
 * made. This is done to avoid the inefficiencies of inserting tokens into the original dynamic array over
 * and over again which causes a need to shift everything to the right by one each time
 *
 * NOTE: This pass is going to replace the token stream that we currently have with a new one that has the
 * macro definitions removed, and has all of the macro replacement sites populated
 */
static u_int8_t macro_replacement_pass(ollie_token_stream_t* stream, macro_symtab_t* macro_symtab){
	//Pointer to the current token in the old array
	lexitem_t* current_token_pointer;

	//The macro record(if one exists)
	symtab_macro_record_t* found_record = NULL;

	//This is the old token array, with all of the macros in it
	ollie_token_array_t* old_array =  &(stream->token_stream);

	//This is the entirely new token array, that we will eventually be parsing in
	//the parser
	ollie_token_array_t new_array = token_array_alloc();

	//The index into the old token array
	u_int32_t old_array_index = 0;

	//So long as we're within the acceptable bounds of the array
	while(old_array_index < old_array->current_index){
		//Extract a pointer to the current token
		current_token_pointer = token_array_get_pointer_at(old_array, old_array_index);

		//Go based on what kind of token this is. If we have an identifier, then
		//that could possibly be a macro for us
		switch(current_token_pointer->tok){
			//If we have an identifier, then there is a chance but not a guarantee
			//that we are performing a macro substitution
			case IDENT:
				//Let's see if we have anything here
				found_record = lookup_macro(macro_symtab, current_token_pointer->lexeme.string);

				//We didn't find a macro name match, which is fine - we'll just
				//treat this like a regular token. We expect that this is the
				//most common case
				if(found_record == NULL){
					//Add it into the new array if we aren't being
					//told to ignore it
					if(current_token_pointer->ignore == FALSE){
						token_array_add(&new_array, current_token_pointer);
					}

					//Bump the old array inde
					old_array_index++;

					//Get out of the case
					break;
				}


				//TODO - perform macro substitution
				perform_macro_substitution();

			//Not an identifier
			default:
				//If we are not told to ignore it, add it into
				//the new array
				if(current_token_pointer->ignore == FALSE){
					token_array_add(&new_array, current_token_pointer);
				}

				//Either way bump the index
				old_array_index++;

				break;
		}
	}

	//If we made it all the way down here then this worked
	return SUCCESS;
}


/**
 * Entry point to the entire preprocessor is here. The preprocessor
 * will traverse the token stream and make replacements as it sees
 * fit with defined macros
 */
preprocessor_results_t preprocess(char* file_name, ollie_token_stream_t* stream){
	//Store the preprocessor results
	preprocessor_results_t results;

	//The stream is always the original stream
	results.stream = stream;

	//Store the file name up top globally
	current_file_name = file_name;

	/**
	 * Step 0: we need a customized macro symtab for ease of lookup. This symtab
	 * will allow us to store everything we need we near O(1) access
	 */
	macro_symtab_t* macro_symtab = macro_symtab_alloc();

	/**
	 * Step 1: perform the initial consumption pass on the token stream. This pass has 2
	 * purposes. First, it will consume all of the macros in our initial token stream and parse
	 * them into usable ollie_macro_t definitions. Second, it will flag all of the tokens that are
	 * involved in that macro as "ignorable". This will cause the second replacement pass to ignore
	 * those tokens when we go through the stream again, avoiding reconsumption
	*/
	u_int8_t consumption_pass_result = macro_consumption_pass(stream, macro_symtab);

	//If we failed here then there's no point in going further
	if(consumption_pass_result == FAILURE){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "Unparseable/invalid macros detected. Please rememdy the errors and recompile", current_line_number);
		goto finalizer;
	}


finalizer:
	//Package with this the errors & warnings
	results.error_count = preprocessor_error_count;
	results.warning_count = preprocessor_warning_count;

	/**
	 * Once done, we no longer need the macro symtab so we can completely deallocate
	 * it
	*/
	macro_symtab_dealloc(macro_symtab);

	//Give the results back
	return results;
}
