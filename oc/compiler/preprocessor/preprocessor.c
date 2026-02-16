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
#include "../utils/stack/lexstack.h"
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

//Grouping stack for parameter checking
static lex_stack_t* grouping_stack;

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
 * "Push back" a token by decrementing the index, and return the prior token
 */
static inline lexitem_t* push_back_token_pointer(ollie_token_array_t* array, u_int32_t* index){
	//Decrement it
	(*index)--;

	//Get the prior token
	lexitem_t* token_pointer = token_array_get_pointer_at(array, *index);

	//Update our line number
	current_line_number = token_pointer->line_num;

	//And give back the prior token
	return token_pointer;
}

// ======================================================== Consumption Pass ========================================================================================

/**
 * Process a macro parameter and add it into the current macro's list of parameters
 *
 * NOTE: By the time we get here, we have already seen the opening L_PAREN
 */
static inline u_int8_t process_macro_parameter(symtab_macro_record_t* macro, ollie_token_array_t* token_array, u_int32_t* index){
	//Get the next token
	lexitem_t* lookahead = get_token_pointer_and_increment(token_array, index);

	//There's only one correct option to see here
	switch(lookahead->tok){
		//We can't see this - it would mean it's empty
		case R_PAREN:
			print_preprocessor_message(MESSAGE_TYPE_ERROR, "Macro parameter lists may not be empty. Remove the paranthesis for an unparameterized macro", lookahead->line_num);
			preprocessor_error_count++;
			return FAILURE;

		//This is the one and only valid thing to see
		case IDENT:
			break;

		//Anything else here is some weird error - we will throw and then get out
		default:
			sprintf(info_message, "Expected identifier in macro parameter list but got %s", lexitem_to_string(lookahead));
			print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, lookahead->line_num);
			preprocessor_error_count++;
			return FAILURE;
	}

	//Flag that we're ignoring
	lookahead->ignore = TRUE;

	//If we make it here then we know that we got a valid ident token as a parameter, but we don't know if it's a duplicate
	//or not. We will check now
	for(u_int32_t i = 0; i < macro->parameters.current_index; i++){
		//Extract the macro token
		lexitem_t* token = token_array_get_pointer_at(&(macro->parameters), i);

		//If these two are equal, then we'll need to fail out because the user cannot duplicate parameters
		if(dynamic_strings_equal(&(token->lexeme), &(lookahead->lexeme)) == TRUE){
			sprintf(info_message, "Macro \"%s\" already has a parameter \"%s\"", macro->name.string, lookahead->lexeme.string);
			print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, lookahead->line_num);
			preprocessor_error_count++;
			return FAILURE;
		}
	}

	//Otherwise we're set so add this into the macro array
	token_array_add(&(macro->parameters), lookahead);

	//If we made it here then this all worked
	return SUCCESS;
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
	//we've already seen the $macro to get here,
	//but we'll catch it just in case
	if(lookahead->tok != MACRO){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "$macro keyword expected before macro declaration", lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//IMPORTANT - flag that this token needs to be ignored by the replacer
	lookahead->ignore = TRUE;

	//Now that we've seen the $macro keyword, we need to see the name
	//of the macro via an identifier
	lookahead = get_token_pointer_and_increment(token_array, index);

	//If we did not see an identifier then we are in bad shape here
	if(lookahead->tok != IDENT){
		sprintf(info_message, "Expected identifier after $macro keyword but got %s", lexitem_to_string(lookahead));
		print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//Let's see if we're able to find this macro record. If we are, then we have an issue because that would
	//be a duplicated name
	symtab_macro_record_t* found_macro = lookup_macro(macro_symtab, lookahead->lexeme.string);

	//Fail case - we have a duplicate
	if(found_macro != NULL){
		sprintf(info_message, "The macro \"%s\" has already been defined. Originally defined on line %d", lookahead->lexeme.string, found_macro->line_number);
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

	//Refresh the lookahead to see if we have any parameters
	lookahead = get_token_pointer_and_increment(token_array, index);

	//If we see an L_PAREN, we will begin processing parameters
	if(lookahead->tok == L_PAREN){
		//Flag that we're ignoring
		lookahead->ignore = TRUE;

		//Push this onto the grouping stack
		push_token(grouping_stack, *lookahead);

		//We have parameters so allocate the space for them
		macro_record->parameters = token_array_alloc();

		//We keep looping so long as we are seeing commas
		while(TRUE){
			//Let the helper process the parameter
			u_int8_t status = process_macro_parameter(macro_record, token_array, index);

			//If this failed then we need to get out
			if(status == FAILURE){
				return FAILURE;
			}

			//Refresh the token
			lookahead = get_token_pointer_and_increment(token_array, index);

			//Flag that we're ignoring this too
			lookahead->ignore = TRUE;

			//There are only two valid options here so we'll process accordingly
			switch(lookahead->tok){
				//If it's a comma go right around
				case COMMA:
					continue;

				//This means that we're done
				case R_PAREN:
					//Just a quick check here
					if(pop_token(grouping_stack).tok != L_PAREN){
						print_preprocessor_message(MESSAGE_TYPE_ERROR, "Mismatched parenthesis detected", lookahead->line_num);
						preprocessor_error_count++;
						return FAILURE;
					}

					goto end_parameter_processing;

				//Anything else here does not work
				default:
					sprintf(info_message, "Comma expected between parameters but saw %s instead", lexitem_to_string(lookahead));
					print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, lookahead->line_num);
					preprocessor_error_count++;
					return FAILURE;
			}
		}

	//Otherwise we found nothing so just push this back and move along
	} else {
		lookahead = push_back_token_pointer(token_array, index);
	}

	//Store how many parameters that we have
	u_int32_t macro_parameter_count = macro_record->parameters.current_index;

end_parameter_processing:
	//Unbounded loop through the entire macro
	while(TRUE){
		//Refresh the lookahead token
		lookahead = get_token_pointer_and_increment(token_array, index);

		//Bump the number of tokens in this macro
		macro_record->total_token_count++;

		//Flag that this needs to be ignored
		lookahead->ignore = TRUE;

		//Based on our token here we'll do a few things
		switch(lookahead->tok){
			//This is bad - there is no such thing as a nested macro and we are already
			//in one
			case MACRO:
				print_preprocessor_message(MESSAGE_TYPE_ERROR, "$macro keyword found inside of a macro definition", lookahead->line_num);
				preprocessor_error_count++;
				return FAILURE;

			//This could be good or bad depending on what we're after
			case ENDMACRO:
				//This is invalid, we cannot have a completely 
				//empty macro
				if(macro_token_array->current_index == 0){
					sprintf(info_message, "Ollie macro \"%s\" is empty and is therefore invalid. Macros must have at least one token in them", macro_record->name.string);
					print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, macro_record->line_number);
					preprocessor_error_count++;
					return FAILURE;
				}

				//Otherwise this should be fine, so we will go ahead and add this on in
				goto finalize_macro;

			/**
			 * If we've seen the done token that is bad. It means that the user never added the $endmacro
			 * binder for the preprocessor to hit. This is also a fail case
			 */
			case DONE:
				sprintf(info_message, "End of file hit. Are you missing a \"$endmacro\" directive for macro \"%s\"?", macro_record->name.string);
				print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, macro_record->line_number);
				preprocessor_error_count++;
				return FAILURE;

			/**
			 * If we have an identifier, there is a chance that this is a macro parameter. If it is, then we're going to
			 * want to flag this here to make future searching easier
			 */
			case IDENT:
				//Run through all of our parameters and see if we have a match
				for(u_int32_t i = 0; i < macro_parameter_count; i++){
					//Extract it
					lexitem_t* parameter = token_array_get_pointer_at(&(macro_record->parameters), i);

					//If these are the same, then we've found a parameter
					if(dynamic_strings_equal(&(parameter->lexeme), &(lookahead->lexeme)) == TRUE){
						//Flag for later processing that this is in fact a macro parameter
						lookahead->tok = MACRO_PARAM;

						/**
						 * Store the parameter number so that we have easy access later on down the road
						 */
						lookahead->constant_values.parameter_number = i;

						//Already found a match so leave
						break;
					}
				}

				//Whatever happened, we need to add the lookahead into the array
				token_array_add(macro_token_array, lookahead);

				break;

			//In theory anything else that we see in here is valid, so we'll
			//just do our bookkeeping and move along
			default:
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
static inline u_int8_t macro_consumption_pass(ollie_token_stream_t* stream, macro_symtab_t* macro_symtab, u_int32_t* num_macros){
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

				//We've seen one more macro here
				(*num_macros)++;

				break;

			//If we see this, that means we have a floating endmacro in there
			case ENDMACRO:
				print_preprocessor_message(MESSAGE_TYPE_ERROR, "Floating $endmacro directive declared. Are you missing a $macro directive?", token->line_num);
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

// ======================================================== Consumption Pass ========================================================================================

// ======================================================== Replacement Pass ========================================================================================

/**
 * The value of a macro parameter may be one or more tokens, and may include a recursive macro subsitution inside of it
 *
 * This function returns an array of tokens that represents the complete subsitution for this given macro parameter. When
 * the caller receives this result, they are going to splice this entire token array onto the end of the final array verbatim. It
 * is for this reason that we can leave no stone unturned here
 */
static ollie_token_array_t* generate_parameter_substitution_array(ollie_token_array_t* old_array, u_int32_t* old_token_array_index){
	//Get a heap allocated array
	ollie_token_array_t* result_array = token_array_heap_alloc();

	//Advance the lookahead here
	lexitem_t* lookahead = get_token_pointer_and_increment(old_array, old_token_array_index);

	//TODO INTEGRATE INTO HERE

	//This is what we give back in the end
	return result_array;
}


/**
 * This rule handles all of the parameter processing for any given macro. This can get complex as ollie allows
 * users to recursively call macros inside of macro parameters themselves
 *
 * For every single parameter, we are going to maintain a token array that represents what that parameter is going to expand to
 *
 * Let's work through an example:
 *
 * $macro EXAMPLE(x, y, z)
 *  y - x + sizeof(z) + x
 * $endmacro
 *
 * pub fn sample(arg1:i32, arg2:i16) -> i32 {
 * 	  let x:i32 = 3333;
 * 	  let y:i32 = 2222;
 *
 * 	  let final_result:i32 = EXAMPLE((arg1 + x), (arg2 - y), arg2);
 * }
 *
 * Let's analyze how example will be handled. We will first note that example is a macro and we need to subsitute.
 * Once we enter into the parameter processing step, we will first hit x
 *
 * Macro paraemeter "x" -> "(arg1 + x)"
 * Macro parameter "y" -> "(arg2 - y)"
 * Macro parameter "z" -> "arg2"
 *
 * So our version of this macro is going to expand to: "(arg2 - y) - (arg1 + x) + sizeof(arg2) + (arg1 + x)"
 * 															y		     x                z           x
 *
 * This expanded version will be created and stored in a token array, then that array will be copy-pasted in place of 
 * the macro call site above
 */
static u_int8_t perform_parameterized_substitution(ollie_token_array_t* target_array, ollie_token_array_t* old_array, u_int32_t* old_token_array_index, symtab_macro_record_t* macro){
	//Store how many parameters this macro has
	u_int32_t parameter_count = macro->parameters.current_index;

	//Otherwise, this macro does have parameters, so we need to process accordingly
	lexitem_t* old_array_lookahead = get_token_pointer_and_increment(old_array, old_token_array_index);

	//We need to see this here
	if(old_array_lookahead->tok != L_PAREN){
		sprintf(info_message, "Macro \"%s\" takes %d parameters. Opening parenthesis is expected", macro->name.string, parameter_count);
		print_preprocessor_message(MESSAGE_TYPE_ERROR, info_message, old_array_lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//Push this onto the grouping stack
	push_token(grouping_stack, *old_array_lookahead);

	//Keep track of the current param number. This is how we index into the array
	u_int32_t current_parameter_number = 0;

	/**
	 * Maintain a 1-to-1 array mapping for the parameter itself to the
	 * token array that we've generated for it
	 */
	ollie_token_array_t* parameter_subsitutions[parameter_count];

	//Run through all of the parameters here
	while(current_parameter_number < parameter_count){
		parameter_subsitutions[current_parameter_number] = per

		//Bump it up
		current_parameter_number++;
	}

	//We now need to see a closing RPAREN
	old_array_lookahead = get_token_pointer_and_increment(old_array, old_token_array_index);
	if(old_array_lookahead->tok != R_PAREN){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "Closing parenthesis expected", old_array_lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//Let's also clean up the grouping stack
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "Unmatched parenthesis detected", old_array_lookahead->line_num);
		preprocessor_error_count++;
		return FAILURE;
	}

	//TODO - the actual substitution part


	//If we got all the way here then this worked
	return SUCCESS;

}


/**
 * Perform a simple macro substitution where we are guaranteed to have no parameters. This function will only be invoked
 * when we know that there are no parameters
 */
static inline u_int8_t perform_non_parameterized_substitution(ollie_token_array_t* target_array, symtab_macro_record_t* macro){
	//Run through all of the tokens in this macro, and splice them over into
	//the target macro
	for(u_int32_t i = 0; i < macro->tokens.current_index; i++){
		//Get a a pointer to this token
		lexitem_t* token_pointer = token_array_get_pointer_at(&(macro->tokens), i);

		//Add it in here - this does do a complete copy
		token_array_add(target_array, token_pointer);
	}

	//This worked so
	return SUCCESS;
}


/**
 * Perform the macro substitution itself. This involves splicing in the
 * token stream that our given macro expands to
 *
 * NOTE: By the time that we get here, we've already seen the macro name and know that this macro does in fact exist
 */
static inline u_int8_t perform_macro_substitution(ollie_token_array_t* target_array, ollie_token_array_t* old_array, u_int32_t* old_token_array_index, symtab_macro_record_t* macro){
	//Store how many parameters this macro has
	u_int32_t parameter_count = macro->parameters.current_index;

	//Does this macro have parameters? If it does not, we are going to perform a regular pass
	if(macro->parameters.current_index == 0){
		return perform_non_parameterized_substitution(target_array, macro);
	} else {
		return perform_parameterized_substitution(target_array, old_array, old_token_array_index, macro);
	}
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
	symtab_macro_record_t* found_macro = NULL;

	//This is the old token array, with all of the macros in it
	ollie_token_array_t* old_token_array =  &(stream->token_stream);

	//This is the entirely new token array, that we will eventually be parsing in
	//the parser
	ollie_token_array_t new_token_array = token_array_alloc();

	//The index into the old token array
	u_int32_t old_token_array_index = 0;

	//So long as we're within the acceptable bounds of the array
	while(old_token_array_index < old_token_array->current_index){
		//Extract a pointer to the current token
		current_token_pointer = token_array_get_pointer_at(old_token_array, old_token_array_index);

		//Bump the index up
		old_token_array_index++;

		/**
		 * Important - if we've been instructed to specifically ignore
		 * this token, then we need to skip over it
		 */
		if(current_token_pointer->ignore == TRUE){
			continue;
		}

		//Go based on what kind of token this is. If we have an identifier, then
		//that could possibly be a macro for us
		switch(current_token_pointer->tok){
			//If we have an identifier, then there is a chance but not a guarantee
			//that we are performing a macro substitution
			case IDENT:
				//Let's see if we have anything here
				found_macro = lookup_macro(macro_symtab, current_token_pointer->lexeme.string);

				//We didn't find a macro name match, which is fine - we'll just
				//treat this like a regular token. We expect that this is the
				//most common case
				if(found_macro == NULL){
					token_array_add(&new_token_array, current_token_pointer);

					//Get out of the case
					break;
				}

				//Use the new array and the macro we found to do our substitution
				u_int8_t substitution_result = perform_macro_substitution(&new_token_array, old_token_array, &old_token_array_index, found_macro);

				//Get out if we have a failure here
				if(substitution_result == FAILURE){
					return FAILURE;
				}

				break;

			//Not an identifier
			default:
				//We know that we aren't ignoring, so just add this to
				//the array
				token_array_add(&new_token_array, current_token_pointer);

				break;
		}
	}

	//At the very end - we will replace the old token stream with the new one
	stream->token_stream = new_token_array;

	//If we made it all the way down here then this worked
	return SUCCESS;
}

// ======================================================== Replacement Pass ========================================================================================

/**
 * Entry point to the entire preprocessor is here. The preprocessor
 * will traverse the token stream and make replacements as it sees
 * fit with defined macros
 */
preprocessor_results_t preprocess(char* file_name, ollie_token_stream_t* stream){
	//Store the preprocessor results
	preprocessor_results_t results;

	//The stream is always the original stream. This is done more so for code
	//flow reasons. We don't expect to actually be modifying this
	results.stream = stream;

	//Initially assume everything worked. This will be flipped if need be
	results.status = PREPROCESSOR_SUCCESS;

	//Store the file name up top globally
	current_file_name = file_name;

	//Allocate the global lex stack for use in both the consumption and replacement passes
	lex_stack_t stack = lex_stack_alloc();

	//This just holds a pointer to it
	grouping_stack = &stack;

	//Keep trace of how many macros we've seen
	u_int32_t num_macros = 0;

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
	u_int8_t consumption_pass_result = macro_consumption_pass(stream, macro_symtab, &num_macros);

	//If we failed here then there's no point in going further
	if(consumption_pass_result == FAILURE){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "Unparseable/invalid macros detected. Please rememdy the errors and recompile", current_line_number);
		//Note a failure
		results.status = PREPROCESSOR_FAILURE;
		goto finalizer;
	}

	/**
	 * If we found no macros at all, then we do not need to do anything with a replacement
	 * pass. This would just be wasteful. Instead, we will just go right to the end
	 */
	if(num_macros == 0){
		goto finalizer;
	}

	/**
	 * Step 2: if we did find macros, then we need to perform a replacement pass. The replacement
	 * pass will do 2 things. First, it will replace all of the macro calls with their appropriate token streams and second, it
	 * will remove all of the macros/macro calls from the token stream. The replacement pass will under the covers
	 * create a secondary token stream object that will replace the original one, which will be deallocated
	 */
	u_int8_t replacement_pass_result = macro_replacement_pass(stream, macro_symtab);

	//This is very rare but if it does happen we will note it
	if(replacement_pass_result == FAILURE){
		print_preprocessor_message(MESSAGE_TYPE_ERROR, "Unparseable/invalid macros detected. Please rememdy the errors and recompile", current_line_number);
		//Note a failure
		results.status = PREPROCESSOR_FAILURE;
	}
	

finalizer:
	//Package with this the errors & warnings
	results.error_count = preprocessor_error_count;
	results.warning_count = preprocessor_warning_count;

	/**
	 * Once done, we no longer need the macro symtab so we can completely deallocate
	 * it. If the user has mistakenly replaced variables with macro names, then that is
	 * on them to figure out
	*/
	macro_symtab_dealloc(macro_symtab);

	//Let's also deallocate the grouping stack
	lex_stack_dealloc(grouping_stack);

	//Give the results back
	return results;
}
