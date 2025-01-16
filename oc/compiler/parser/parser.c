/**
 * The parser for Ollie-Lang
 *
 * GOAL: The goal of the parser is to determine if the input program is a syntatically valid sentence in the language.
 * This is done via recursive-descent in our case. As the 
 *
 * OVERALL STRUCTURE: The parser is the second thing that sees the source code. It only acts upon token streams that are given
 * to it from the lexer. The parser's goal is twofold. It will ensure that the structure of the program adheres to the rules of
 * the programming language, and it will translate the source code into an "Intermediate Representation(IR)" that can be given to 
 * the optimizer
 *
 * This parser will do both parsing AND elaboration of macros in the future(not yet supported)
*/

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

//Variable and function symbol tables
function_symtab_t* function_symtab;
variable_symtab_t* variable_symtab;
type_symtab_t* type_symtab;


//Our stack for storing variables, etc
heap_stack_t* grouping_stack;

//The number of errors
u_int16_t num_errors = 0;

//The current parser line number
u_int16_t parser_line_num = 1;

//Are we in a param_list?
u_int8_t in_param_list = 0;

//What's the function that we're in currently?
symtab_function_record_t* current_function = NULL;

//The current IDENT that we are tracking
Lexer_item* current_ident = NULL;

//The current type. Used for global access
generic_type_t* active_type = NULL;

//The root of the entire tree
generic_ast_node_t* ast_root = NULL;


//Function prototypes are predeclared here as needed to avoid excessive restructuring of program
static u_int8_t cast_expression(FILE* fl);
static u_int8_t assignment_expression(FILE* fl);
static u_int8_t conditional_expression(FILE* fl);
static u_int8_t unary_expression(FILE* fl);
static u_int8_t type_specifier(FILE* fl, generic_ast_node_t* parent);
static u_int8_t declaration(FILE* fl);
static u_int8_t compound_statement(FILE* fl);
static u_int8_t statement(FILE* fl);
static u_int8_t expression(FILE* fl);
static u_int8_t initializer(FILE* fl);
static u_int8_t declarator(FILE* fl);
static u_int8_t direct_declarator(FILE* fl);


/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_type_t message_type, char* info, u_int16_t line_num){
	//Build and populate the message
	parse_message_t parse_message;
	parse_message.message = message_type;
	parse_message.info = info;
	parse_message.line_num = line_num;

	//Fatal if error
	if(message_type == PARSE_ERROR){
		parse_message.fatal = 1;
	}

	//Now print it
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	printf("[LINE %d: PARSER %s]: %s\n", parse_message.line_num, type[parse_message.message], parse_message.info);
}


/**
 * An identifier itself is always a child, never a parent. As such, we can
 * handle the node attachment here
 *
 * BNF "Rule": <identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static u_int8_t identifier(FILE* fl, generic_ast_node_t* parent_node){
	//In case of error printing
	char info[2000];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num);
	
	//If we can't find it that's bad
	if(lookahead.tok != IDENT){
		sprintf(info, "String %s is not a valid identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return 0;
	}

	//Create the identifier node
	generic_ast_node_t* ident_node = ast_node_alloc(AST_NODE_CLASS_IDENTIFER);

	//Add the identifier into the node itself
	strcpy(((identifier_ast_node_t*)(ident_node->node))->identifier, lookahead.lexeme);

	//Add this into the tree
	add_child_node(parent_node, ident_node);

	return 1;
}


/**
 * Do we have a label identifier or not?
 */
static u_int8_t label_identifier(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	char info[2000];
	
	//If we can't find it that's bad
	if(l.tok != LABEL_IDENT){
		sprintf(info, "String %s is not a valid label identifier", l.lexeme);
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		return 0;
	}

	//We'll push this ident onto the stack and let whoever called(function/variable etc.) deal with it
	//We have no need to search the symtable in this function because we are unable to context-sensitive
	//analysis here
	return 1;
}


/**
 * Pointers can be chained(several *'s at once)
 *
 * BNF Rule: <pointer> ::= * {<pointer>}?
 */
static u_int8_t pointer(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	generic_type_t* temp;

	//Grab the star
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok == STAR){
		//We've seen a pointer, so now we need to handle a pointer
		//Save the reference
		temp = active_type;

		//We'll also store the active type in the symtable to maintain it's information
		insert_type(type_symtab, create_type_record(temp));
		
		//Create a pointer type that points to temp
		active_type = create_pointer_type(temp, current_line);
		
		//Refresh the token, continue the search
		lookahead = get_next_token(fl, &parser_line_num);
		//If we see another pointer, handle it
		if(lookahead.tok == STAR){
			//Put it back for the next rule to handle
			push_back_token(fl, lookahead);
			return pointer(fl);
		} else {
			//Put back and leave
			push_back_token(fl, lookahead);
			return 1;
		}

	} else {
		//Put it back and get out, it's not catastrophic if we don't see it
		push_back_token(fl, lookahead);
		return 0;
	}
}


/**
 * Handle a constant. There are 4 main types of constant, all handled by this function
 *
 * BNF Rule: <constant> ::= <integer-constant> 
 * 						  | <string-constant> 
 * 						  | <float-constant> 
 * 						  | <char-constant>
 */
static u_int8_t constant(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item l;
	
	//We should see one of the 4 constants here
	l = get_next_token(fl, &parser_line_num);

	//Do this for now, later on we'll need symtable integration
	if(l.tok == INT_CONST || l.tok == STR_CONST || l.tok == CHAR_CONST || l.tok == FLOAT_CONST ){
		return 1;
	}

	//Otherwise here, we have an error
	print_parse_message(PARSE_ERROR, "Invalid constant found", current_line);
	num_errors++;
	return 0;
}

/**
 * A prime rule that allows for comma chaining and avoids right recursion
 *
 * REMEMBER: by the time that we've gotten here, we've already seen a comma
 *
 * BNF Rule: <expression_prime> ::= , <assignment-expression><expression_prime>
 */
static u_int8_t expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status;

	//We must first see a valid assignment-expression
	status = assignment_expression(fl);
	
	//It failed
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid assignment expression found in expression", current_line);
		num_errors++;
		return 0;
	}

	//If we see a comma now, we know that we've triggered the prime rule
	lookahead = get_next_token(fl, &parser_line_num);

	//Go on to the prime rule
	if(lookahead.tok == COMMA){
		return expression_prime(fl);
	} else {
		//Put it back and bail out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An expression decays into an assignment expression and can be chained using commas
 *
 * BNF Rule: <expression> ::= <assignment-expression> 
 * 							| <assignment-expression><expression_prime>
 */
static u_int8_t expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status;

	//We must first see a valid assignment-expression
	status = assignment_expression(fl);
	
	//It failed
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid assignment expression found in expression", current_line);
		num_errors++;
		return 0;
	}

	//If we see a comma now, we know that we've triggered the prime rule
	lookahead = get_next_token(fl, &parser_line_num);

	//Go on to the prime rule
	if(lookahead.tok == COMMA && in_param_list == 0){
		return expression_prime(fl);
	} else {
		//Put it back and bail out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A primary expression is, in a way, the termination of our expression chain. However, it can be used 
 * to chain back up to an expression in general using () as an enclosure
 *
 * BNF Rule: <primary-expression> ::= <identifier>
 * 									| <constant> 
 * 									| (<expression>)
 */
static u_int8_t primary_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's grab the token that we next have
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have an ident, we'll call the appropriate function
	if(lookahead.tok == IDENT){
		//Push it back and call ident
		push_back_token(fl, lookahead);

		status = identifier(fl);

		//If it failed
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid identifier found in primary expression", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise we're all set
		return 1;
	//If we see a constant, we'll call the appropriate function to handle it
	} else if(lookahead.tok == CHAR_CONST || lookahead.tok == INT_CONST 
		   || lookahead.tok == STR_CONST || lookahead.tok == FLOAT_CONST){
		//Push it back and call constant
		push_back_token(fl, lookahead);
		
		status = constant(fl);

		//If it failed
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid constant found in primary expression", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise we're all set
		return 1;
	//If we see an l_paren, we're gonna have another expression
	} else if(lookahead.tok == L_PAREN){
		//Push for later
		push(grouping_stack, lookahead);
		
		//We now must see a valid expression
		status = expression(fl);

		//If it failed
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid expression found in primary expression", current_line);
			num_errors++;
			return 0;
		}

		//We must now also see an R_PAREN, and ensure that we have matching on the stack
		lookahead = get_next_token(fl, &parser_line_num);

		//If it isn't a right parenthesis
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression", current_line);
			num_errors++;
			return 0;
		//Make sure we match
		} else if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise we're all set
		return 1;
	} else {
		char info[2000];
		memset(info, 0, 2000 * sizeof(char));
		sprintf(info, "Invalid token with lexeme %s found in primary expression", lookahead.lexeme); 
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		return 0;
	}
}


/**
 * An assignment expression can decay into a conditional expression or it
 * can actually do assigning. There is no chaining in Ollie language of assignments
 *
 * BNF Rule: <assignment-expression> ::= <conditional-expression> 
 * 									   | asn <unary-expression> := <conditional-expression>
 */
static u_int8_t assignment_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//We've seen the ASN keyword
	if(lookahead.tok == ASN){
		//Since we've seen this, we now need to see a valid unary expression
		status = unary_expression(fl);

		//We have a bad one
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid unary expression found in assignment expression", current_line);
			num_errors++;
			return 0;
		}

		//Now we must see the := assignment operator
		lookahead = get_next_token(fl, &parser_line_num);

		//We have a bad one
		if(lookahead.tok != COLONEQ){
			print_parse_message(PARSE_ERROR, "Assignment operator := expected after unary expression", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise it worked just fine
		//Now we must see an conditional expression again
		status = conditional_expression(fl);

		//We have a bad one
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid conditional expression found in assingment expression", current_line);
			num_errors++;
			return 0;
		}

		//All went well if we get here
		return 1;
		
	} else {
		//Put it back if not
		push_back_token(fl, lookahead);
		//We have a conditional expression
		status = conditional_expression(fl);

		//We have a bad one
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid conditional expression found in postfix expression", current_line);
			num_errors++;
			return 0;
		}
		
		
		//Otherwise it worked
		return 1;
	}
}


/**
 * A postfix expression decays into a primary expression, and there are certain
 * operators that can be chained if context allows
 *
 * BNF Rule: <postfix-expression> ::= <primary-expression> 
 * 									| <primary-expression>:<postfix-expression> 
 * 									| <primary-expression>::<postfix-expression> 
 * 									| <primary-expression>{[ <expression> ]}*
 * 									| <primary-expression>{[ <expression> ]}*:<postifx-expression> 
 * 									| <primary-expression>{[ <expression> ]}*::<postfix-expression> 
 * 									| <primary-expression> ( {<conditional-expression>}* {, <conditional-expression>}*  ) 
 * 									| <primary-expression> ++ 
 * 									| <primary-expression> --
 */ 
static u_int8_t postfix_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	char info[2000];
	char function_name[100];
	//0 this out
	memset(function_name, 0, 100*sizeof(char));
	u_int8_t status = 0;

	//We must first see a valid primary expression no matter what
	status = primary_expression(fl);

	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid primary expression found in postifx expression", current_line);
		num_errors++;
		return 0;
	}

	//Othwerise we're good to move on, so we'll need to lookahead here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//There are a multitude of different things that we could see here
	switch (lookahead.tok) {
		//If we see these then we're done
		case MINUSMINUS:
		case PLUSPLUS:
			//TODO handle this later
			//All set here
			return 1;
	
		//These are our memory addressing schemes
		case COLON:
		case DOUBLE_COLON:
			//If we see these, we know that we'll need to make a recursive call
			//TODO handle the actual memory addressing later on
			return postfix_expression(fl);

		//If we see a left paren, we are looking at a function call
		case L_PAREN:
			//Push to the stack for later
			push(grouping_stack, lookahead);

			//How many inputs have we seen?
			u_int8_t params_seen = 0;

			//Copy it in for safety
			strcpy(function_name, current_ident->lexeme);
			//This is for sure a function call, so we need to be able to recognize the function
			symtab_function_record_t* func = lookup_function(function_symtab, function_name);

			//Let's see if we found it
			if(func == NULL){
				//Wipe it
				memset(info, 0, 2000*sizeof(char));
				//Format nice
				sprintf(info, "Function \"%s\" was not defined", current_ident->lexeme);
				//Relese the memory
				free(current_ident);
				print_parse_message(PARSE_ERROR, info, current_line);
				num_errors++;
				return 0;
			}

			//Release these here
			free(current_ident);
			//Let's check to see if we have an immediate end
			lookahead = get_next_token(fl, &parser_line_num);

			//If it is an R_PAREN
			if(lookahead.tok == R_PAREN){
				goto end_params;
			} else {
				//Otherwise put it back
				push_back_token(fl, lookahead);
			} 

			//Loop until we see the end
			//As long as we don't see a right paren
			while(1){
				//Now we need to see a valid conditional-expression
				status = conditional_expression(fl);

				//Bail out if bad
				if(status == 0){
					print_parse_message(PARSE_ERROR,  "Invalid conditional expression given to function call", current_line);
					num_errors++;
					return 0;
				}
				
				//One more param seen
				params_seen++;

				//Grab the next token here
				lookahead = get_next_token(fl, &parser_line_num);
				
				//If it's not a comma get out
				if(lookahead.tok != COMMA){
					break;
				}
			}
			
			//Once we break out here, in theory our token will be a right paren
			//Just to double check
			if(lookahead.tok != R_PAREN){
				print_parse_message(PARSE_ERROR, "Right parenthesis at the end of function call", current_line);
				num_errors++;
				return 0;
			}
			
		end_params:
			//Check for matching
			if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
				num_errors++;
				return 0;
			}

			//Now check for parameter correctness TODO NOT DONE
			if(params_seen != func->number_of_params){
				//Special printing details here
				memset(info, 0, 2000*sizeof(char));
				sprintf(info, "Function \"%s\" requires %d parameters, was given %d. Function first defined here:", func->func_name, func->number_of_params, params_seen);
				print_parse_message(PARSE_ERROR, info, current_line);
				print_function_name(func);

				num_errors++;
				return 0;
			}

			//If we make it here, then we should be all in the clear
			return 1;

		//If we see a left bracket, we then need to see an expression
		case L_BRACKET:
			//As long as we see left brackets
			while(lookahead.tok == L_BRACKET){
				//Push it onto the stack
				push(grouping_stack, lookahead);

				//We must see a valid expression
				status = expression(fl);
				
				//We have a bad one
				if(status == 0){
					print_parse_message(PARSE_ERROR, "Invalid expression in primary expression index", current_line);
					num_errors++;
					return 0;
				}

				//Now we have to see a valid right bracket
				lookahead = get_next_token(fl, &parser_line_num);

				//Just to double check
				if(lookahead.tok != R_BRACKET){
					print_parse_message(PARSE_ERROR, "Right bracket expected after primary expression index", current_line);
					num_errors++;
					return 0;
				//Or we have some unmatched grouping operator
				} else if(pop(grouping_stack).tok != L_BRACKET){
					print_parse_message(PARSE_ERROR, "Unmatched bracket detected", current_line);
					num_errors++;
					return 0;
				}
				
				//Now we must refresh the lookahead
				lookahead = get_next_token(fl, &parser_line_num);
			}

			//Once we break out here, we no longer have any left brackets	
			//We could however see the colon or double_colon operators, in which case we'd make a recursive call
			if(lookahead.tok == COLON || lookahead.tok == DOUBLE_COLON){
				//Return the postfix expression here
				return postfix_expression(fl);
			}
		
			//Otherwise we don't know what it is, so we'll get out
			push_back_token(fl, lookahead);
			return 1;

		//It is possible to see nothing afterwards, so we'll just get out if this is the case
		default:
			//Whatever we saw we didn't use, so put it back
			push_back_token(fl, lookahead);
			return 1;
	}
}


/**
 * A unary expression decays into a postfix expression
 *
 * BNF Rule: <unary-expression> ::= <postfix-expression> 
 * 								  | ++<unary-expression> 
 * 								  | --<unary-expression> 
 * 								  | <unary-operator> <cast-expression> 
 * 								  | size (<unary-expression>)
 * 								  | typesize (<type-name>)
 *
 * Note that we will expand the Unary-operator rule here, as there is no point in having
 * a separate function for it
 *
 * <unary-operator> ::= & 
 * 					  | * 
 * 					  | ` 
 * 					  | + 
 * 					  | - 
 * 					  | ~ 
 * 					  | ! 
 */
static u_int8_t unary_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's first see what we have as the next token
	lookahead = get_next_token(fl, &parser_line_num);

	switch (lookahead.tok) {
		//If we see either of these, we must next see a valid unary expression
		case MINUSMINUS:
		case PLUSPLUS:
			//Let's see if we have it
			status = unary_expression(fl);

			//If it was bad
			if(status == 0){
				//print_parse_message(PARSE_ERROR, "Invalid unary expression following preincrement/predecrement", current_line);
				num_errors++;
				return 0;
			}
		
			//If we make it here we know it went well
			return 1;

		//If we see the size keyword
		case SIZE:
			//We must then see a left parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			if(lookahead.tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Left parenthesis expected after size keyword", current_line);
				num_errors++;
				return 0;
			}
			
			//Push it onto the stack
			push(grouping_stack, lookahead);
			
			//Now we must see a valid unary expression
			status = unary_expression(fl);

			//If it was bad
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid unary expression given to size operator", current_line);
				num_errors++;
				return 0;
			}
		
			//If we get here though we know it worked
			//Now we must see a valid closing parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			//If this is an R_PAREN
			if(lookahead.tok != R_PAREN) {
				print_parse_message(PARSE_ERROR, "Right parenthesis expected after unary expression", current_line);
				num_errors++;
				return 0;
			//Otherwise if it wasn't matched right
			} else if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
				num_errors++;
				return 0;
			}
		
			//Otherwise, we're all good here
			return 1;

		//If we see the typesize keyword
		case TYPESIZE:
			//We must then see a left parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			if(lookahead.tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Left parenthesis expected after typesize keyword", current_line);
				num_errors++;
				return 0;
			}
			
			//Push it onto the stack
			push(grouping_stack, lookahead);
			
			//Now we must see a valid type name 
			status = type_name(fl);

			//If it was bad
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid type name given to typesize operator", current_line);
				num_errors++;
				return 0;
			}
		
			//If we get here though we know it worked
			//Now we must see a valid closing parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			//If this is an R_PAREN
			if(lookahead.tok != R_PAREN) {
				print_parse_message(PARSE_ERROR, "Right parenthesis expected after type name", current_line);
				num_errors++;
				return 0;
			//Otherwise if it wasn't matched right
			} else if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
				num_errors++;
				return 0;
			}
		
			//Otherwise, we're all good here
			return 1;

		//We could also see one of our unary operators
		case PLUS:
		case MINUS:
		case STAR:
		case AND:
		case B_NOT:
		case L_NOT:
			//No matter what we see here, we will have to see a valid cast expression after it
			status = cast_expression(fl);

			//If it was bad
			if(status == 0){
				//print_parse_message(PARSE_ERROR, "Invalid cast expression following unary operator", current_line);
				num_errors++;
				return 0;
			}

			//If we get here then we know it worked
			return 1;
		
		//If we make it all the way down here, we have to see a postfix expression
		default:
			//Whatever we saw, we didn't use, so push it back
			push_back_token(fl, lookahead);
			//No matter what we see here, we will have to see a valid cast expression after it
			status = postfix_expression(fl);

			//If it was bad
			if(status == 0){
				//print_parse_message(PARSE_ERROR, "Invalid postfix expression inside of unary expression", current_line);
				num_errors++;
				return 0;
			}

			//If we get here then we know it worked
			return 1;
	}
}



/**
 * A cast expression decays into a unary expression
 *
 * BNF Rule: <cast-expression> ::= <unary-expression> 
 * 						    	| < <type-name> > <unary-expression>
 */
static u_int8_t cast_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's see if we have a parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have an L_PAREN, we'll push it to the stack
	if(lookahead.tok == L_THAN){
		//Push this on for later
		push(grouping_stack, lookahead);
		
		//We now must see a valid type name
		status = type_name(fl);
		
		//TODO symtab integration
		
		//If it was bad
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid type name found in cast expression", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise we're good to keep going
		//We now must see a valid R_PAREN
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If this is an R_PAREN
		if(lookahead.tok != G_THAN) {
			print_parse_message(PARSE_ERROR, "Angle brackets expected after type name", current_line);
			num_errors++;
			return 0;
		} else if(pop(grouping_stack).tok != L_THAN){
			print_parse_message(PARSE_ERROR, "Unmatched angle brackets detected", current_line);
			num_errors++;
			return 0;
		}
		//Otherwise it all went well here
	} else {
		//If we didn't see an L_PAREN, put it back
		push_back_token(fl, lookahead);
	}

	//Whether we saw the type-name or not, we now must see a valid unary expression
	status = unary_expression(fl);
	
	//If it was bad
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid unary expression found in cast expression", current_line); 
		num_errors++;
		return 0;
	}

	return 1;
}


/**
 * A prime rule that allows us to avoid direct left recursion
 *
 * REMEMBER: By the time that we get here, we've already seen a *, / or %
 *
 * BNF Rule: <multiplicative-expression-prime> ::= *<cast-expression><multiplicative-expression-prime> 
 * 												 | /<cast-expression><multiplicative-expression-prime> 
 * 												 | %<cast-expression><multiplicative-expression-prime>
 */
static u_int8_t multiplicative_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid cast expression
	status = cast_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid cast expression found in multiplicative expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see *, /  or % here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a + or a - we can make a recursive call
	if(lookahead.tok == STAR || lookahead.tok == MOD || lookahead.tok == F_SLASH){
		return multiplicative_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A multiplicative expression can be chained and decays into a cast expression
 *
 * BNF Rule: <multiplicative-expression> ::= <cast-expression> 
 * 										   | <cast-expression><multiplicative-expression-prime>
 */
static u_int8_t multiplicative_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid cast expression
	status = cast_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid cast expression found in multiplicative expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see *, /  or % here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a + or a - we can make a recursive call
	if(lookahead.tok == STAR || lookahead.tok == MOD || lookahead.tok == F_SLASH){
		return multiplicative_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A prime rule to avoid left recursion
 *
 * REMEMBER: By the time we get here, we've already seen a plus or minus
 *
 * BNF Rule: <additive-expression-prime> ::= + <multiplicative-expression><additive-expression-prime> 
 * 										   | - <multiplicative-expression><additive-expression-prime>
 */
static u_int8_t additive_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//First we must see a valid multiplicative expression
	status = multiplicative_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid mutliplicative expression found in additive expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see * or / here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a + or a - we can make a recursive call
	if(lookahead.tok == MINUS || lookahead.tok == PLUS){
		return additive_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * Additive expression can be chained and as such require a prime rule. They decay into multiplicative
 * expressions
 *
 * BNF Rule: <additive-expression> ::= <multiplicative-expression> 
 * 									 | <multiplicative-expression><additive-expression-prime>
 */
static u_int8_t additive_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//First we must see a valid multiplicative expression
	status = multiplicative_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid multiplicative expression found in additive expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see * or / here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a + or a - we can make a recursive call
	if(lookahead.tok == MINUS || lookahead.tok == PLUS){
		return additive_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A shift expression cannot be chained, so no recursion is needed here. It decays into an additive expression
 *
 * BNF Rule: <shift-expression> ::= <additive-expression> 
 *								 |  <additive-expression> << <additive-expression> 
 *								 |  <additive-expression> >> <additive-expression>
 */
static u_int8_t shift_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid additive expression
	status = additive_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid additive expression found in shift expression", current_line);
		num_errors++;
		return 0;
	}

	//Let's see if we have any shift operators
	lookahead = get_next_token(fl, &parser_line_num);

	//Are there any shift operators?
	if(lookahead.tok != L_SHIFT && lookahead.tok != R_SHIFT){
		//If not, put it back and leave
		push_back_token(fl, lookahead);
		return 1;
	}
	
	//If we get here, we now have to see a valid additive expression
	status = additive_expression(fl);

	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid additive expression found in shift expression", current_line);
		num_errors++;
		return 0;
	}
	
	//If we get to here then we're all good
	return 1;
}


/**
 * A relational expression will descend into a shift expression. Ollie language does not allow for
 * chaining in relational expressions, no recursion will occur here.
 *
 * <relational-expression> ::= <shift-expression> 
 * 						     | <shift-expression> > <shift-expression> 
 * 						     | <shift-expression> < <shift-expression> 
 * 						     | <shift-expression> >= <shift-expression> 
 * 						     | <shift-expression> <= <shift-expression>
 */
static u_int8_t relational_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid shift expression
	status = shift_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid shift expression found in relational expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see one of our relational operators, then we can get out
	if(lookahead.tok != G_THAN && lookahead.tok != L_THAN
	  && lookahead.tok != G_THAN_OR_EQ && lookahead.tok != L_THAN_OR_EQ){
		//Put it back and leave
		push_back_token(fl, lookahead);
		return 1;
	}
	
	//Otherwise, we now must see another valid shift expression
	//We must first see a valid shift expression
	status = shift_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid shift expression found in relational expression", current_line);
		num_errors++;
		return 0;
	}
	
	//If we get to here then we're all good
	return 1;
}


/**
 * An equality expression can be chained and descends into a relational expression 
 *
 * BNF Rule: <equality-expression> ::= <relational-expression> 
 * 									 | <relational-expression> == <relational-expression>
 * 									 | <relational-expression> != <relational-expression>
 */
static u_int8_t equality_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid relational-expression
	status = relational_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid relational expression found in equality expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a == or a != we can make a recursive call
	if(lookahead.tok == D_EQUALS || lookahead.tok == NOT_EQUALS){
		//We now need to see another relational expression
		status = relational_expression(fl);

		//Fail out
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid relational expression in equality expression", current_line);
			num_errors++;
			return 0;
		}

		//Success otherwise
		return 1;

	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A prime rule that we use to avoid direct left recursion
 *
 * REMEMBER: By the time that we get here, we've already seen a '&'
 * 
 * BNF Rule: <and-expression-prime> ::= &<equality-expression><and-expression-prime>
 */
static u_int8_t and_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid equality-expression
	status = equality_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid equality expression found in and expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have see an and(&) we can make the recursive call
	if(lookahead.tok == AND){
		return and_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An and-expression descends into an equality expression and can be chained
 *
 * BNF Rule: <and-expression> ::= <equality-expression> 
 * 								| <equality-expression><and-expression-prime>
 */
static u_int8_t and_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid equality-expression
	status = equality_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid equality expression found in and expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have see an and(&) we can make the recursive call
	if(lookahead.tok == AND){
		return and_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A prime rule to avoid direct left recursion
 *
 * Remember, by the time that we've gotten here, we've already seen the ^ operator
 *
 * BNF Rule: <exclusive-or-expression-prime> ::= ^<and_expression><exlcusive-or-expression-prime>
 */
static u_int8_t exclusive_or_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid and-expression
	status = and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid and expression found in exclusive or expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have see a carrot(^) we can make the recursive call
	if(lookahead.tok == CARROT){
		return exclusive_or_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An exclusive or expression can be chained, and descends into an and-expression
 *
 * BNF Rule: <exclusive-or-expression> ::= <and-expression> 
 * 										 | <and_expression><exclusive-or-expression-prime>
 */
static u_int8_t exclusive_or_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid and-expression
	status = and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid and expression found in exclusive or expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the carrot(^) here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have see a carrot(^) we can make the recursive call
	if(lookahead.tok == CARROT){
		return exclusive_or_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A prime rule to avoid direct left recursion
 *
 * REMEMBER: By the time that we've gotten here, we've already seen the (|) terminal
 *
 * BNF Rule: <inclusive-or-expression-prime> ::= |<exclusive-or-expression><inclusive-or-expression-prime>
 */
u_int8_t inclusive_or_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid exclusive or expression
	status = exclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid exclusive or expression found in inclusive or expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have see a pipe(|) we can make the recursive call
	if(lookahead.tok == OR){
		return inclusive_or_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An inclusive or expression can be chained, and descends into an exclusive or
 * expression
 *
 * BNF rule: <inclusive-or-expression> ::= <exclusive-or-expression><inclusive-or-expression-prime>
 */
static u_int8_t inclusive_or_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = exclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid exclusive or expression found in inclusive or expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have see a pipe(|) we can make the recursive call
	if(lookahead.tok == OR){
		return inclusive_or_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A prime nonterminal that will allow us to avoid left recursion
 *
 * REMEMBER: By the time that we get here, we've already seen the "&&" terminal
 *
 * BNF Rule: <logical-and-expression-prime> ::= &&<inclusive-or-expression><logical-and-expression-prime>
 */
u_int8_t logical_and_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = inclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid inclusive or expression found in logical and expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have a double and we'll make a recursive call
	if(lookahead.tok == DOUBLE_AND){
		return logical_and_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A logical and expression can also be chained together, and it descends into
 * an inclusive or expression
 *
 * BNF Rule: <logical-and-expression> ::= <logical-and-expression> ::= <inclusive-or-expression><logical-and-expression-prime>
 */
u_int8_t logical_and_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = inclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid inclusive or expression found in logical expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have a double and we'll make a recursive call
	if(lookahead.tok == DOUBLE_AND){
		return logical_and_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A prime nonterminal that will allow us to avoid left recursion. 
 *
 * REMEMBER: By the time that we've gotten here, we've already seen the "||" terminal
 *
 * BNF Rule: <logical-or-expression-prime ::= ||<logical-and-expression><logical-or-expression-prime> 
 */
u_int8_t logical_or_expression_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status;

	//We now must see a valid logical and expression
	status = logical_and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid logical and expression found in logical or expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double || here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//Then we have a double or, so we'll make a recursive call
	if(lookahead.tok == DOUBLE_OR){
		return logical_or_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A logical or expression can be chained together as many times as we want, and
 * descends into a logical and expression
 * BNF Rule: <logical-or-expression> ::= <logical-and-expression><logical-or-expression-prime>
 */
u_int8_t logical_or_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We first must see a valid logical and expression
	status = logical_and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid logical and expression found in logical or expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double || here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//Then we have a double or, so we'll make a recursive call
	if(lookahead.tok == DOUBLE_OR){
		return logical_or_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * A conditional expression is simply used as a passthrough for a logical or expression,
 * but some important checks may be done here so we'll use it
 *
 * BNF Rule: <conditional-expression> ::= <logical-or-expression>
 */
u_int8_t conditional_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Pass through to the logical or expression expression
	u_int8_t status = logical_or_expression(fl);

	//Something failed
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid logical or expression found in conditional expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise we're all set
	return 1;
}


/**
 * A constant expression is simply used as a passthrough for a conditional expression,
 * but some important checks may be performed here so that's why we have it
 * BNF Rule: <constant-expression> ::= <conditional-expression> 
 */
u_int8_t constant_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Pass through to the conditional expression
	u_int8_t status = conditional_expression(fl);

	//Something failed
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid conditional expression found in constant expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise we're all set
	return 1;
}


/**
 * A structure declarator is grammatically identical to a regular declarator
 *
 * BNF Rule: <construct-declarator> ::= <declarator> 
 *
 */
u_int8_t construct_declarator(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We can see a declarator
	status = declarator(fl);

	//TODO by no means done

	//Otherwise we're all set so return 1
	return 1;
}

/**
 * A construct declaration can optionally be chained into a large list
 *
 * BNF Rule: <construct-declaration> ::= {constant}? <type-specifier> <construct-declarator>
 */
u_int8_t construct_declaration(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We can see the constant keyword here optionally
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see constant keyword
	if(lookahead.tok == CONSTANT){
		//TODO handle
	} else {
		//Put back
		push_back_token(fl, lookahead);
	}
	
	//We must see a valid one
	status = type_specifier(fl);

	//Fail out if bad
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid type specifier in structure declaration", current_line);
		num_errors++;
		return 0;
	}

	//Now we must see a valid structure declarator
	status = construct_declarator(fl);

	//Fail out if bad
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid structure declarator in structure declaration", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise it worked so
	return 1;
}

/**
 * A construct definer is the definition of a construct 
 *
 * REMEMBER: By the time we get here, we've already seen the construct keyword
 *
 * NOTE: The caller will do the final insertion into the symbol table
 *
 * BNF Rule: <construct-specifier> ::= construct <ident> { <construct-declaration> {, <construct-declaration>}* } 
 */
static u_int8_t construct_definer(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;
	//For error printing
	char info[2000];

	//The name of the construct type
	char construct_name[MAX_TYPE_NAME_LENGTH];

	//Copy the name in here
	strcpy(construct_name, "construct ");

	//We now have to see a valid identifier, since we've already seen the construct keyword
	//Stored here in current ident global variable
	status = identifier(fl);

	//If we don't see an ident
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid identifier found in construct specifier", parser_line_num);
		num_errors++;
		return 0;
	}

	//Otherwise, we'll add this into our name
	strcat(construct_name, current_ident->lexeme);

	//Now in this case, it would be bad if it does exist
	symtab_type_record_t* type = lookup_type(type_symtab, construct_name);

	//If it does exist, we're done here 
	if(type != NULL){
		sprintf(info, "Constructed type with name \"%s\" already exists. First defined here:", construct_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_type_name(type);
		num_errors++;
		return 0;
	}

	//We now must see a left curly to officially start defining
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Fail out here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Raw definitions are not allowed, construct must be fully defined in definition statement", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise we saw a left curly, so push to stack 
	push(grouping_stack, lookahead);

	//Create the type
	generic_type_t* constructed_type = create_constructed_type(construct_name, current_line);

	//Set the active type to be this type
	active_type = constructed_type;

	//Now we must see a valid structure declaration
	status = construct_declaration(fl);

	//If we failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid construct declaration inside of construct definition", current_line);
		num_errors++;
		return 0;
	}

	//We can optionally see a comma here
	lookahead = get_next_token(fl, &parser_line_num);

	//As long as we see commas
	while(lookahead.tok == COMMA){
		//We must now see a valid declaration
		status = construct_declaration(fl);

		//If we fail
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid construct declaration inside of construct definition", current_line);
			num_errors++;
			return 0;
		}	

		//Refresh lookahead
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Once we get here it must be a closing curly
	//If we don't see a curly
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Right curly brace expected after structure declaration", current_line);
		num_errors++;
		return 0;
	}

	//If it's unmatched
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise it worked so
	return 1;

}


/**
 * A construct specifier is the entry to a construct 
 *
 * REMEMBER: By the time we get here, we've already seen the construct keyword
 *
 * BNF Rule: <construct-specifier> ::= construct <ident>
 */
static u_int8_t construct_specifier(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;
	//For error printing
	char info[2000];

	//The name of the construct type
	char construct_name[MAX_TYPE_NAME_LENGTH];

	//Copy the name in here
	strcpy(construct_name, "construct ");

	//We now have to see a valid identifier, since we've already seen the construct keyword
	//Stored here in current ident global variable
	status = identifier(fl);

	//If we don't see an ident
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid identifier found in construct specifier", parser_line_num);
		num_errors++;
		return 0;
	}

	//Otherwise, we'll add this into our name
	strcat(construct_name, current_ident->lexeme);

	//Now once we get here, we need to check and see if this construct actually exists
	symtab_type_record_t* type = lookup_type(type_symtab, construct_name);

	//If it doesn't exist, we're done here 
	if(type == NULL){
		sprintf(info, "Constructed type with name \"%s\" does not exist", construct_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return 0;
	}

	//Otherwise we made it here and we're all clear
	active_type = type->type;

	return 1;
}


/**
 * An enumerator here is simply an identifier. Ollie language does not support custom
 * indexing for enumerated types
 *
 * BNF Rule: <enumerator> ::= <identifier> 
 */
u_int8_t enumerator(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	//For error printing
	char info[2000];

	//We will use these for checking duplicate names 
	symtab_function_record_t* function_record;
	symtab_type_record_t* type_record;
	symtab_variable_record_t* variable_record;

	u_int8_t status = 0;

	//We must see a valid identifier here
	status = identifier(fl);

	//Get out if bad
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid identifier in enumerator", current_line);
		num_errors++;
		return 0;
	}

	//Something very strange if this happens
	if(active_type->type_class != TYPE_CLASS_ENUMERATED){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Enumerated type not active in enumerator", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise it worked, so we now have a current IDENT that is valid
	//However we aren't done, because we now must check for duplicates everywhere
	function_record = lookup_function(function_symtab,  current_ident->lexeme);

	//Name collision here
	if(function_record != NULL){
		sprintf(info, "A function with the name \"%s\" was already defined. Enumeration members and functions may not share names. First declared here:", current_ident->lexeme);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_function_name(function_record);
		num_errors++;
		return 0;
	}

	//Let's check for variable collisions
	variable_record = lookup_variable(variable_symtab, current_ident->lexeme);
		
	//Name collision here
	if(variable_record != NULL){
		sprintf(info, "A variable with the name \"%s\" was already defined. Enumeration members and variables may not share names. First declared here:", current_ident->lexeme);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_variable_name(variable_record);
		num_errors++;
		return 0;
	}

	//Let's check for type collisions finally
	type_record = lookup_type(type_symtab, current_ident->lexeme); 

	//Name collision here
	if(type_record != NULL){
		sprintf(info, "A type with the name \"%s\" was already defined. Enumeration members and types may not share names. First declared here:", current_ident->lexeme);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_type_name(type_record);
		num_errors++;
		return 0;
	}

	/**
	 * Now if we make it here we'll have to construct a variable and place it in. For the needs of ollie lang, enumeration members are considered
	 * a special kind of variable. These variables contain references to the fact that they are declared in enumerations
	 */
	variable_record = create_variable_record(current_ident->lexeme, STORAGE_CLASS_NORMAL);

	//This flag tells us where we are
	variable_record->is_enumeration_member = 1;

	//Assign the type to be the enumerated type
	variable_record->type = active_type;
	//It was initialized
	variable_record->initialized = 1;
	//Store the line num
	variable_record->line_number = parser_line_num;

	//Insert this into the symtab
	insert_variable(variable_symtab, variable_record);

	//We now link this in here
	active_type->enumerated_type->tokens[active_type->enumerated_type->size] = variable_record;
	//One more token
	active_type->enumerated_type->size++;

	return 1;
}


/**
 * Helper to maintain RL(1) properties. Remember, by the time we've gotten here, we've already seen a COMMA
 *
 * BNF Rule: <enumerator-list-prime> ::= ,<enumerator><enumerator-list-prime>
 */
u_int8_t enumeration_list_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We now need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid enumerator in enumeration list", current_line);
		num_errors++;
		return 0;
	}

	//Now if we see a comma, we know that we have an enumerator-list-prime
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a comma, we'll use the helper
	if(lookahead.tok == COMMA){
		return enumeration_list_prime(fl);
	} else {
		//Put it back and get out if no
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An enumeration list guarantees that we have at least one enumerator
 *
 * BNF Rule: <enumerator-list> ::= <enumerator><enumerator-list-prime>
 */
u_int8_t enumeration_list(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid enumerator in enumeration list", current_line);
		num_errors++;
		return 0;
	}

	//Now if we see a comma, we know that we have an enumerator-list-prime
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a comma, we'll use the helper
	if(lookahead.tok == COMMA){
		return enumeration_list_prime(fl);
	} else {
		//Put it back and get out if no
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An enumeration definition is where we see the actual definition of an enum
 *
 * NOTE: The actual addition into the symtable is handled by the caller
 *
 * BNF Rule: enumerated <identifier> { <enumerator-list> } 
 */
static u_int8_t enumeration_definer(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Info array for error printing
	char info[2000];

	//Used for finding duplicates
	symtab_type_record_t* type_record;

	//The name of the enumerated type
	char enumerated_name[MAX_TYPE_NAME_LENGTH];

	//For grabbing tokens
	Lexer_item lookahead;

	//Copy the name in here
	strcpy(enumerated_name, "enumerated ");

	//We now have to see a valid identifier, since we've already seen the ENUMERATED keyword
	//Stored here in current ident global variable
	u_int8_t status = identifier(fl);

	//If it's bad then we're done here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid enumeration name given in definition", current_line);
		num_errors++;
		return 0;
	}

	//If we found a valid ident, we'll add it into the name
	//Following this, we'll have the whole name of the enumerated type written out
	strcat(enumerated_name, current_ident->lexeme);

	//This means that the type must have been defined, so we'll check
	type_record = lookup_type(type_symtab, enumerated_name);

	//If we couldn't find it
	if(type_record != NULL){
		sprintf(info, "Enumerated type \"%s\" has already been defined, redefinition is illegal", enumerated_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_type_name(type_record);
		num_errors++;
		return 0;
	}

	//Following this, if we see a right curly brace, we know that we have a list
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Enumeration defintion expected after the name is defined", current_line);
		num_errors++;
		return 0;
	}

	//Push onto the grouping stack for matching
	push(grouping_stack, lookahead);

	//Before we even go on, if this was already defined, we can't have it
	type_record = lookup_type(type_symtab, enumerated_name);

	//If it is already defined, we'll bail out
	if(type_record != NULL){
		//Automatic fail case
		sprintf(info, "Illegal type redefinition. Enumerated type %s was already defined here:", enumerated_name);
		print_parse_message(PARSE_ERROR, info, current_line); 
		print_type_name(type_record);
		return 0;
	}

	//Once we get here we know that we're declaring so we can create
	generic_type_t* type = create_enumerated_type(enumerated_name, current_line);

	//This now is the active type
	active_type = type;

	//We now must see a valid enumeration list
	status = enumeration_list(fl);

	//If it's bad then we're done here
	if(status == 0){
		num_errors++;
		return 0;
	}

	//Must see a right curly
	lookahead = get_next_token(fl, &parser_line_num);

	//All of our fail cases here
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Right curly brace expected at end of enumeration list", current_line);
		num_errors++;
		return 0;
	}

	//Unmatched left curly
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched right parenthesis", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise we should be fine when we get here, so we can return
	return 1;
}


/**
 * An enumeration specifier will always start with enumerated
 * REMEMBER: Due to RL(1), by the time we get here ENUMERATED has already been seen
 *
 * BNF Rule: <enumeration-specifier> ::= enumerated <identifier>
 */
u_int8_t enumeration_specifier(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Info array for error printing
	char info[2000];

	//Used for finding duplicates
	symtab_type_record_t* type_record;

	//The name of the enumerated type
	char enumerated_name[MAX_TYPE_NAME_LENGTH];

	//For grabbing tokens
	Lexer_item lookahead;

	//Copy the name in here
	strcpy(enumerated_name, "enumerated ");

	//We now have to see a valid identifier, since we've already seen the ENUMERATED keyword
	//Stored here in current ident global variable
	u_int8_t status = identifier(fl);

	//If it's bad then we're done here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid enumeration name given in declaration", current_line);
		num_errors++;
		return 0;
	}

	//If we found a valid ident, we'll add it into the name
	//Following this, we'll have the whole name of the enumerated type written out
	strcat(enumerated_name, current_ident->lexeme);

	//This means that the type must have been defined, so we'll check
	type_record = lookup_type(type_symtab, enumerated_name);

	//If we couldn't find it
	if(type_record == NULL){
		sprintf(info, "Enumerated type \"%s\" is either not defined or being used before declaration", enumerated_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		return 0;
	}

	//Assign the active type
	active_type = type_record->type;
	
	return 1;
}



static u_int8_t type_address_specifier(FILE* fl, generic_ast_node_t* type_specifier, symtab_type_record_t** current_record){

}



/**
 * A type name node is always a child of a type specifier. It consists
 * of all of our primitive types and any defined construct or
 * aliased types that we may have. It is important to note that any
 * non-primitive type needs to have been previously defined for it to be
 * valid
 * 
 * Also note that no checking against the type symbol table will be done in
 * this function
 *
 * BNF Rule: <type-name> ::= void 
 * 						   | u_int8 
 * 						   | s_int8 
 * 						   | u_int16 
 * 						   | s_int16 
 * 						   | u_int32 
 * 						   | s_int32 
 * 						   | u_int64 
 * 						   | s_int64 
 * 						   | float32 
 * 						   | float64 
 * 						   | char 
 * 						   | enumerated <identifer>
 * 						   | construct <identifier>
 * 						   | <identifier>
 */
static u_int8_t type_name(FILE* fl, generic_ast_node_t* type_specifier){
	//Global status
	u_int8_t status = 0;
	//Lookahead token
	Lexer_item lookahead;

	//Let's create the type name node
	generic_ast_node_t* type_name = ast_node_alloc(AST_NODE_CLASS_TYPE_NAME);

	//It will always be a child of the type specifier node
	add_child_node(type_specifier,  type_name);

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num);
	
	//These are all of our basic types
	if(lookahead.tok == VOID || lookahead.tok == U_INT8 || lookahead.tok == S_INT8 || lookahead.tok == U_INT16
	   || lookahead.tok == S_INT16 || lookahead.tok == U_INT32 || lookahead.tok == S_INT32 || lookahead.tok == U_INT64
	   || lookahead.tok == S_INT64 || lookahead.tok == FLOAT32 || lookahead.tok == CHAR){

		//Copy the lexeme into the node
		strcpy(((type_name_ast_node_t*)type_name->node)->type_name, lookahead.lexeme);

		//This one is all set now
		return 1;

	//Otherwise we may have an enumerated type
	} else if(lookahead.tok == ENUMERATED){
		//Add in the enumerated keyword
		strcpy(((type_name_ast_node_t*)type_name->node)->type_name, "enumerated ");

		//Now we have to see a valid identifier. The parent of this identifer
		//Will itself be the type_name node
		status = identifier(fl, type_name);

		//If this is the case we'll fail out, no need for a message
		if(status == 0){
			return 0;
		}

		//If we make it here we know that it's valid, so we'll grab the identifier name
		//and add it to our name
		strcat(((type_name_ast_node_t*)type_name->node)->type_name, ((identifier_ast_node_t*)type_name->first_child->node)->identifier);

		//Once we have this, we're out of here
		return 1;

	//Construct names are pretty much the same as enumerated names
	} else if(lookahead.tok == CONSTRUCT){
		//Add in the enumerated keyword
		strcpy(((type_name_ast_node_t*)type_name->node)->type_name, "construct ");

		//Now we have to see a valid identifier. The parent of this identifer
		//Will itself be the type_name node
		status = identifier(fl, type_name);

		//If this is the case we'll fail out, no need for a message
		if(status == 0){
			return 0;
		}

		//If we make it here we know that it's valid, so we'll grab the identifier name
		//and add it to our name
		strcat(((type_name_ast_node_t*)type_name->node)->type_name, ((identifier_ast_node_t*)type_name->first_child->node)->identifier);

		//Once we have this, we're out of here
		return 1;
	
	//If this is the case then we have to see some user defined name, which is an ident
	} else {
		//We'll put this token back into the stream
		push_back_token(fl, lookahead);

		//Now we have to see a valid identifier. The parent of this identifer
		//Will itself be the type_name node
		status = identifier(fl, type_name);

		//If this is the case we'll fail out, no need for a message
		if(status == 0){
			return 0;
		}

		//If we make it here we know that it's valid, so we'll grab the identifier name
		//and add it to our name
		strcat(((type_name_ast_node_t*)type_name->node)->type_name, ((identifier_ast_node_t*)type_name->first_child->node)->identifier);

		//Once we have this, we're out of here
		return 1;
	}
}



/**
 * A type specifier is a type name that is then followed by an address specifier, this being  
 * the array brackets or address indicator. A type specifier is always the child of some
 * global parent
 *
 * The type specifier itself is comprised of some type name and potential address specifiers
 *
 * BNF Rule: <type-specifier> ::= <type-name>{<type-address-specifier>}*
 */
static u_int8_t type_specifier(FILE* fl, generic_ast_node_t* parent){
	//For error printing
	char info[2000];
	//Freeze the current line
	u_int8_t current_line = parser_line_num;
	//Global status var
	u_int8_t status = 0;

	//We'll first create and attach the type specifier node
	//At this point the node will be entirely blank
	generic_ast_node_t* type_spec_node = ast_node_alloc(AST_NODE_CLASS_TYPE_SPECIFIER);

	//We'll attach it as a child to the parent
	add_child_node(parent, type_spec_node);

	//Now we'll hand off the rule to the <type-name> function. The type name function will
	//add a new child node to the type_spec_node, which we will later use in the type
	//record creation
	status = type_name(fl, type_spec_node);

	//Throw and get out
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid type name given to type specifier", current_line);
		return 0;
	}

	//Just for convenience, we'll store this locally
	char type_name[MAX_TYPE_NAME_LENGTH];
	//The name will be in the type_spec_nodes first child
	strcpy(type_name, ((type_name_ast_node_t*)((type_spec_node->first_child)->node))->type_name);

	//We'll now lookup the type that we have and keep it as a temporary reference
	//We're also checking for existence. If this type does not exist, then that's bad
	symtab_type_record_t* current_type = lookup_type(type_symtab, type_name);

	//This is a "leaf-level" error
	if(current_type == NULL){
		sprintf(info, "Type with name: \"%s\" does not exist in the current scope.", type_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		return 0;
	}

	//Now if we make it here, we know that the type exists in the system, and we have a record of it
	//in our hands. We can now optionally see some type-address-specifiers. These take the form of
	//array brackets or address operators(&)
	//
	//The type-address-specifier function works uniquely compared to other functions. It will actively
	//modify the type that we have currently active. When it's done, our "current_type" reference should
	//in theory be fully done with arrays
	//
	//Just like before, this node is the child of the type-spec-node
	status = type_address_specifier(fl, type_spec_node, &current_type);

	




}



/**
 * A parameter declaration is always a child of a parameter list node. It can optionally
 * be made constant. The register keyword is not needed here. Ollie lang restricts the 
 * number of parameters to 6 so that they all may be kept in registers ideally(minus large structs)
 *
 * A parameter declaration is always a parent to other nodes
 *
 * BNF Rule: <parameter-declaration> ::= {constant}? <type-specifier> <identifier>
 */
static u_int8_t parameter_declaration(FILE* fl, generic_ast_node_t* parameter_list_node){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;

	//Is it constant? No by default
	u_int8_t is_constant = 0;

	//Lookahead for peeking
	Lexer_item lookahead;

	//General status var
	u_int8_t status = 0;

	//We'll first create and attach the actual parameter declaration node
	generic_ast_node_t* parameter_decl_node = ast_node_alloc(AST_NODE_CLASS_PARAM_DECL);

	//This node will always be a child of the parent level parameter list
	add_child_node(parameter_list_node, parameter_decl_node);

	//Increment the parameter list node count
	((param_list_ast_node_t*)(parameter_list_node->node))->num_params++;

	//Now we can optionally see constant here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Is this parameter constant? If so we'll just set a flag for later
	if(lookahead.tok == CONSTANT){
		is_constant = 1;
	} else {
		//Put it back and move on
		push_back_token(fl, lookahead);
		is_constant = 0;
	}

	//Now we must see a valid type specifier
	status = type_specifier(fl, parameter_decl_node);
	
	//If it's bad then we're done here
	if(status == 0){
		num_errors++;
		return 0;
	}

	//Finally, we must see a direct declarator that is valid
	status = parameter_direct_declarator(fl);

	//If it's bad then we're done here
	if(status == 0){
		num_errors++;
		return 0;
	}

	//After this call we should have the name of the ident that the parameter is
	symtab_variable_record_t* var = create_variable_record(current_ident->lexeme, storage_class);
	//Store the active type
	var->type = active_type;
	//This is a function param so we'll keep it here
	var->parent_function = current_function;
	var->is_function_paramater = 1;

	//Store in the symtab
	insert_variable(variable_symtab, var);
	
	//One more parameter
	if(current_function == NULL){
		print_parse_message(PARSE_ERROR,  "Internal parse error at parameter declaration", current_line);
		num_errors++;
		//Cleanup here
		free(current_ident);
		return 0;
	}

	//We'll also link this in with the function
	current_function->func_params[current_function->number_of_params].associate_var = var;
	//Increment for us
	(current_function->number_of_params)++;

	//Cleanup here
	free(current_ident);
	active_type = NULL;

	return 1;
}


/**
 * Optional repetition allowed with our parameter list
 *
 * REMEMBER: By the time that we get here, we've already seen a comma
 *
 * BNF Rule: <parameter-list-prime> ::= , <parameter-declaration><parameter-list-prime>
 * 									  | epsilon
 */
u_int8_t parameter_list_prime(FILE* fl, generic_ast_node_t* param_list_node){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//If we see a comma, we will proceed
	lookahead = get_next_token(fl, &parser_line_num);

	//If there's no comma, we'll just bail out
	if(lookahead.tok != COMMA){
		//This is our "epsilon" case
		return 1;
	}

	//Otherwise, we can now see a valid declaration
	//This declarations parent will be the parameter list that 
	//was carried through here
	status = parameter_declaration(fl, param_list_node);

	//If we didn't see a valid one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid parameter declaration in parameter list", current_line);
		num_errors++;
		return 0;
	}

	//We can keep going as long as there are parameters
	return parameter_list_prime(fl, param_list_node);
}


/**
 * A parameter list will always be the child of a function node
 *
 * <parameter-list> ::= <parameter-declaration><parameter-list-prime>
 * 					  | epsilon
 */
u_int8_t parameter_list(FILE* fl, generic_ast_node_t* parent){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We initialize this scope automatically, even if there is no param list.
	//It will just be empty if this is the case, no big issue
	initialize_variable_scope(variable_symtab);

	//Let's now create the parameter list node and add it into the tree
	generic_ast_node_t* param_list_node = ast_node_alloc(AST_NODE_CLASS_PARAM_LIST);

	//This will be the child of the function node
	add_child_node(parent, param_list_node);

	//There are two options that we have here. We can have an entirely blank
	//parameter list. 
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see an R_PAREN, it's blank so we'll just leave
	if(lookahead.tok == R_PAREN){
		return 1;
	} else {
		//Otherwise we'll put the token back and keep going
		push_back_token(fl, lookahead);
	}

	//First, we must see a valid parameter declaration. Here, the parent will be the
	//parameter list
	status = parameter_declaration(fl, param_list_node);
	
	//If we didn't see a valid one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid parameter declaration in parameter list", current_line);
		num_errors++;
		return 0;
	}

	//Again here the parent is the parameter list node
	return parameter_list_prime(fl, param_list_node);
}


/**
 * BNF Rule: <expression-statement> ::= {<expression>}?;
 */
static u_int8_t expression_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's see if we have a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//Empty expression, we're done here
	if(lookahead.tok == SEMICOLON){
		return 1;
	}

	//Otherwise, put it back and call expression
	push_back_token(fl, lookahead);
	
	//We now must see a valid expression
	status = expression(fl);

	//Fail case
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid expression discovered", current_line);
		num_errors++;
		return 0;
	}

	//Now to close out we must see a semicolon
	//Let's see if we have a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//Empty expression, we're done here
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after statement", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise we're all set
	return 1;
}


/**
 * <labeled-statement> ::= <label-identifier> <compound-statement>
 * 						 | case <constant-expression> <compound-statement>
 * 						 | default <compound-statement>
 */
static u_int8_t labeled_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status;

	//Let's grab the next item here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a label identifier
	if(lookahead.tok == LABEL_IDENT){
		//Push it back and process it
		push_back_token(fl, lookahead);
		//Process it
		label_identifier(fl);

		//Now we can see a compound statement
		status = compound_statement(fl);

		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid compound statement in labeled statement", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise it worked so
		return 1;

	//If we see the CASE keyword
	} else if (lookahead.tok == CASE){
		//Now we need to see a constant expression
		status = constant_expression(fl);

		//If it failed
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid constant expression in case statement", current_line);
			num_errors++;
			return 0;
		}
		//Now we can see a compound statement
		status = compound_statement(fl);

		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid compound statement in case statement", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise it worked so
		return 1;


	//If we see the DEFAULT keyword
	} else if(lookahead.tok == DEFAULT){
		//Now we can see a compound statement
		status = compound_statement(fl);

		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid compound statement in case statement", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise it worked so
		return 1;

	//Fail case here
	} else {
		//print_parse_message(PARSE_ERROR, "Invalid keyword for labeled statement", current_line);
		num_errors++;
		return 0;
	}
}


/**
 * The callee will have left the if token for us once we get here
 *
 * BNF Rule: <if-statement> ::= if( <expression> ) then <compound-statement> {else <if-statement | compound-statement>}*
 */
static u_int8_t if_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	Lexer_item lookahead2;
	u_int8_t status = 0;

	//First we must see the if token
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't see it fail out
	if(lookahead.tok != IF){
		print_parse_message(PARSE_ERROR, "if keyword expected in if statement", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we now must see parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after if statement", current_line);
		num_errors++;
		return 0;
	}

	//Push onto the stack
	push(grouping_stack, lookahead);
	
	//We now need to see a valid expression
	status = expression(fl);

	//If we fail
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid expression found in if statement", current_line); 
		num_errors++;
		return 0;
	}

	//Following the expression we need to see a closing paren
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see the R_Paren
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression in if statement", current_line);
		num_errors++;
		return 0;
	}

	//Now let's check the stack
	if(pop(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
		num_errors++;
		return 0;
	}

	//If we make it to this point, we need to see the THEN keyword
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out if bad
	if(lookahead.tok != THEN){
		print_parse_message(PARSE_ERROR, "then keyword expected following expression in if statement", current_line);
		num_errors++;
		return 0;
	}

	//Now we must see a valid compound statement
	status = compound_statement(fl);

	//If we fail
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid compound statement in if block", current_line);
		num_errors++;
		return 0;
	}

	//Once we're here, we can optionally see the else keyword repeatedly
	//Seed the search
	lookahead = get_next_token(fl, &parser_line_num);

	//As long as we keep seeing else
	while(lookahead.tok == ELSE){
		//Grab the next guy
		lookahead = get_next_token(fl, &parser_line_num);

		//We can either see an if statement or a compound statement
		if(lookahead.tok == IF){
			//Put it back
			push_back_token(fl, lookahead);

			//Call if statement if we see this
			status = if_statement(fl);

			//If we fail
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid else-if block", current_line);
				num_errors++;
				return 0;
			}

		} else {
			//We have to see a compound statement here
			//Push the token back
			push_back_token(fl, lookahead);

			//Let's see if we have one
			status = compound_statement(fl);

			//If we fail
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid compound statement in else block", current_line);
				num_errors++;
				return 0;
			}
		}

		//Once we make it down here, we'll refresh the search to see what we have next
		lookahead = get_next_token(fl, &parser_line_num);
	}
	
	//We escaped so push it back and leave
	push_back_token(fl, lookahead);
	return 1;
}


/**
 * BNF Rule: <jump-statement> ::= jump <label-identifier>;
 * 								| continue when(<conditional-expression>); 
 * 								| continue; 
 * 								| break when(<conditional-expression>); 
 * 								| break; 
 * 								| ret {<conditional-expression>}?;
 */
static u_int8_t jump_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a jump statement
	if(lookahead.tok == JUMP){
		//We now must see a valid label-ident
		status = label_identifier(fl);

		//Fail out
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid label identifier found after jump statement", current_line);
			num_errors++;
			return 0;
		}
		//semicolon handled at end
		
	} else if(lookahead.tok == CONTINUE){
		//Grab the next toekn because we could have "continue when"
		lookahead = get_next_token(fl, &parser_line_num);

		//If we have continue when
		if(lookahead.tok != WHEN){
			//Regular continue here, go to semicolon
			push_back_token(fl, lookahead);
			//TODO handle accordingly
			goto semicol;
		}

		//Otherwise, we must see parenthesis here
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after when keyword", current_line);
			num_errors++;
			return 0;
		}

		//Push to stack for later
		push(grouping_stack, lookahead);

		//Now we must see a valid conditional expression
		status = conditional_expression(fl);

		//fail out
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid conditional expression in continue when statement", current_line);
			num_errors++;
			return 0;
		}

		//Finally we must see a closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//If we don't see it
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after conditional expression", current_line);
			num_errors++;
			return 0;
		}

		//Double check that we matched
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise we're good to go
	
	} else if(lookahead.tok == BREAK){
		//Grab the next toekn because we could have "break when"
		lookahead = get_next_token(fl, &parser_line_num);

		//If we have continue when
		if(lookahead.tok != WHEN){
			//Regular continue here, go to semicolon
			push_back_token(fl, lookahead);
			//TODO handle accordingly
			goto semicol;
		}

		//Otherwise, we must see parenthesis here
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after when keyword", current_line);
			num_errors++;
			return 0;
		}

		//Push to stack for later
		push(grouping_stack, lookahead);

		//Now we must see a valid conditional expression
		status = conditional_expression(fl);

		//fail out
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid conditional expression in continue when statement", current_line);
			num_errors++;
			return 0;
		}

		//Finally we must see a closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//If we don't see it
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after conditional expression", current_line);
			num_errors++;
			return 0;
		}

		//Double check that we matched
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise we're good to go

	} else if(lookahead.tok == RET){
		//A return statement can have an expression at the end
		lookahead = get_next_token(fl, &parser_line_num);

		//We may just have a semicolon here
		if(lookahead.tok == SEMICOLON){
			//TODO handle
			return 1;
		}

		//Otherwise we must see a valid expression
		push_back_token(fl, lookahead);

		//Now we must see a valid conditional-expression
		status = conditional_expression(fl);

		//If we fail
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid conditional expression in ret statement", current_line);
			num_errors++;
			return 0;
		}
		//otherwise we're all set
	}

semicol:
	//We now must see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected at the end of statement", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise all went well
	return 1;
}


/**
 * BNF Rule: <switch-statement> ::= switch on( <expression> ) {<labeled-statement>*}
 */
static u_int8_t switch_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != SWITCH){
		print_parse_message(PARSE_ERROR, "switch keyword expected in switch statement", current_line);
		num_errors++;
		return 0;
	}

	//Now we have to see the on keyword
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != ON){
		print_parse_message(PARSE_ERROR, "on keyword expected after switch in switch statement", current_line);
		num_errors++;
		return 0;
	}

	//Now we must see an lparen
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after on keyword", current_line);
		num_errors++;
		return 0;
	}

	//Push to stack for later
	push(grouping_stack, lookahead);

	//Now we must see a valid expression
	status = expression(fl);

	//Invalid one
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid expression in switch statement", current_line);
		num_errors++;
		return 0;
	}

	//Now we must see a closing paren
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression", current_line);
		num_errors++;
		return 0;
	}

	if(pop(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
		num_errors++;
		return 0;
	}

	//Now we must see an lcurly
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly brace expected after expression", current_line);
		num_errors++;
		return 0;
	}

	//Push to stack for later
	push(grouping_stack, lookahead);

	//Seed the search here
	lookahead = get_next_token(fl, &parser_line_num);

	//So long as there is no closing curly
	while(lookahead.tok != R_CURLY){
		//Fail cases here
		if(lookahead.tok != CASE && lookahead.tok != DEFAULT){
			//print_parse_message(PARSE_ERROR, "Invalid label statement found in switch statement", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise, we must see a labeled statement
		push_back_token(fl, lookahead);

		//Let's see if we have a valid one
		status = labeled_statement(fl);

		//Invalid here
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid label statement found in switch statement", current_line);
			num_errors++;
			return 0;
		}

		//Reseed the search
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//By the time we get here, we should've seen an R_paren
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Closing curly brace expected", current_line);
		num_errors++;
		return 0;
	}

	//We could also have unmatched curlies
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise if we get here, all went well
	return 1;
}


/**
 * Iterative statements encompass while, for and do while loops
 *
 * BNF Rule: <iterative-statement> ::= while( <expression> ) do <compound-statement> 
 * 									 | do <compound-statement> while( <expression> );
 * 									 | for( {<expression>}? ; {<expression>}? ; {<expression>}? ) do <compound-statement>
 */
static u_int8_t iterative_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's see what kind we have here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If we have a while loop
	if(lookahead.tok == WHILE){
		//We must then see parenthesis
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after on keyword", current_line);
			num_errors++;
			return 0;
		}

		//Push to stack for later
		push(grouping_stack, lookahead);

		//Now we must see a valid expression
		status = expression(fl);

		//Invalid one
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid expression in switch statement", current_line);
			num_errors++;
			return 0;
		}

		//Now we must see a closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression", current_line);
			num_errors++;
			return 0;
		}

		//Unmatched parenthesis
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}

		//Now we must see a do keyword
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If we don't see it
		if(lookahead.tok != DO){
			print_parse_message(PARSE_ERROR, "Do keyword expected after expression in while loop", current_line);
			num_errors++;
			return 0;
		}

		//Following that, we must see a valid compound statement
		status = compound_statement(fl);

		//Last fail case
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid compound statement in while loop", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise it worked so
		return 1;

	//Do while loop
	} else if(lookahead.tok == DO){
		//We must immediately see a valid compound statement
		status = compound_statement(fl);

		//Fail out
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid compound statement in do while loop", current_line);
			num_errors++;
			return 0;
		}

		//Now we have to see the while keyword
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out
		if(lookahead.tok != WHILE){
			print_parse_message(PARSE_ERROR, "While keyword expected in do while loop", current_line);
			num_errors++;
			return 0;
		}

		//We must then see parenthesis
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after on keyword", current_line);
			num_errors++;
			return 0;
		}

		//Push to stack for later
		push(grouping_stack, lookahead);

		//Now we must see a valid expression
		status = expression(fl);

		//Invalid one
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid expression in switch statement", current_line);
			num_errors++;
			return 0;
		}

		//Now we must see a closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression", current_line);
			num_errors++;
			return 0;
		}

		//Unmatched parenthesis
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}

		//Finally we need to see a semicolon
		lookahead = get_next_token(fl, &parser_line_num);

		//Final fail case
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected at the end of statement", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise it all worked here
		return 1;

	//For loop case
	} else if(lookahead.tok == FOR){
		//We must then see parenthesis
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after on keyword", current_line);
			num_errors++;
			return 0;
		}

		//Push to stack for later
		push(grouping_stack, lookahead);

		//Now we can either see an expression or a SEMICOL
		lookahead = get_next_token(fl, &parser_line_num);

		//We must then see an expression
		if(lookahead.tok != SEMICOLON){
			//Put it back and find the expression
			push_back_token(fl, lookahead);

			status = expression(fl);

			//Fail case
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid expression found in for loop", current_line);
				num_errors++;
				return 0;
			}

			//Now we do have to see a semicolon
			lookahead = get_next_token(fl, &parser_line_num);

			if(lookahead.tok != SEMICOLON){
				print_parse_message(PARSE_ERROR, "Semicolon expected after expression in for loop", current_line);
				num_errors++;
				return 0;
			}
		}

		//Otherwise it was a semicolon and we have no expression
		//We'll now repeat the exact process for the second one
		//Now we can either see an expression or a SEMICOL
		lookahead = get_next_token(fl, &parser_line_num);

		//We must then see an expression
		if(lookahead.tok != SEMICOLON){
			//Put it back and find the expression
			push_back_token(fl, lookahead);

			status = expression(fl);

			//Fail case
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid expression found in for loop", current_line);
				num_errors++;
				return 0;
			}

			//Now we do have to see a semicolon
			lookahead = get_next_token(fl, &parser_line_num);

			if(lookahead.tok != SEMICOLON){
				print_parse_message(PARSE_ERROR, "Semicolon expected after expression in for loop", current_line);
				num_errors++;
				return 0;
			}
		}

		//Otherwise it was a semicolon and we have no expression
		
		//Finally we can see a third expression or a closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//Let's see if there's a final expression
		if(lookahead.tok != R_PAREN){
			//Put it back for the search
			push_back_token(fl, lookahead);

			status = expression(fl);

			//Fail case
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid expression found in for loop", current_line);
				num_errors++;
				return 0;
			}

			//Now we need to see an R_PAREN
			lookahead = get_next_token(fl, &parser_line_num);

			if(lookahead.tok != R_PAREN){
				print_parse_message(PARSE_ERROR, "Closing parenthesis expected", current_line);
				num_errors++;
				return 0;
			}
		}

		//Once we get here, we know that we had an R_PAREN
		//Let's now check for matching
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}

		//Now we need to see the do keyword
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out here
		if(lookahead.tok != DO){
			print_parse_message(PARSE_ERROR, "Do keyword expected in for loop", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise if we get here, the last thing that we need to see is a valid compound statement
		status = compound_statement(fl);
		
		//Fail case
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid compound statement found in iterative statement", current_line);
			num_errors++;
			return 0;
		}

		//Otherwise we're all set
		return 1;

	//Some weird error
	} else {
		print_parse_message(PARSE_ERROR, "Invalid keyword used for iterative statement", current_line); 
		num_errors++;
		return 0;
	}
}


/**
 * A statement is a kind of multiplexing rule that just determines where we need to go to
 *
 * BNF Rule: <statement> ::= <labeled-statement> 
 * 						   | <expression-statement> 
 * 						   | <compound-statement> 
 * 						   | <if-statement> 
 * 						   | <switch-statement> 
 * 						   | <iterative-statement> 
 * 						   | <jump-statement>
 */
static u_int8_t statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's grab the next item and see what we have here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have a compound statement
	if(lookahead.tok == L_CURLY){
		//We'll put the curly back and let compound statement handle it
		push_back_token(fl, lookahead);
		
		//Let compound statement handle it
		status = compound_statement(fl);

		//If it fails
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid compound statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;

	//If we see a labeled statement
	} else if(lookahead.tok == LABEL_IDENT || lookahead.tok == CASE || lookahead.tok == DEFAULT){
		//Put it back for the actual rule to handle
		push_back_token(fl, lookahead);
		
		//Let this handle it
		status = labeled_statement(fl);

		//If it fails
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid labeled statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;

	//If statement
	} else if(lookahead.tok == IF){
		//Put it back for the actual rule to handle
		push_back_token(fl, lookahead);
		
		//Let this handle it
		status = if_statement(fl);

		//If it fails
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid if statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;

	//Switch statement
	} else if(lookahead.tok == SWITCH){
		//Put it back for the actual rule to handle
		push_back_token(fl, lookahead);
		
		//Let this handle it
		status = switch_statement(fl);

		//If it fails
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid switch statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;

	//Jump statement
	} else if(lookahead.tok == JUMP || lookahead.tok == BREAK || lookahead.tok == CONTINUE
			 || lookahead.tok == RET){
		//Put it back for the actual rule to handle
		push_back_token(fl, lookahead);
		
		//Let this handle it
		status = jump_statement(fl);

		//If it fails
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid jump statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;

	//Iterative statement
	} else if(lookahead.tok == DO || lookahead.tok == WHILE || lookahead.tok == FOR){
		//Put it back for the actual rule to handle
		push_back_token(fl, lookahead);
		
		//Let this handle it
		status = iterative_statement(fl);

		//If it fails
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid iterative statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;

	//Otherwise we just have the generic expression rule here
	} else {
		//Put it back for the actual rule to handle
		push_back_token(fl, lookahead);
		
		//Let this handle it
		status = expression_statement(fl);

		//If it fails
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid expression statement found in statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so get out
		return 1;
	}
}


/**
 * A compound statement is denoted by the {} braces, and can decay in to 
 * statements and declarations
 *
 * BNF Rule: <compound-statement> ::= {{<declaration>}* {<statement>}*}
 */
static u_int8_t compound_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//When we get here, we absolutely must see a cury brace
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Opening curly brace expected to begin compound statement", current_line);
		num_errors++;
		return 0;
	}

	//We'll push this guy onto the stack for later
	push(grouping_stack, lookahead);
	//TODO change the lexical scope here
	
	//Grab the next token to search
	lookahead = get_next_token(fl, &parser_line_num);

	//Now we keep going until we see the closing curly brace
	while(lookahead.tok != R_CURLY && lookahead.tok != DONE){
		//If we see this we know that we have a declaration
		if(lookahead.tok == LET || lookahead.tok == DECLARE || lookahead.tok == DEFINE){
			//Push it back
			push_back_token(fl, lookahead);

			//Hand it off to the declaration function
			status = declaration(fl);
			
			//If we fail here just leave
			if(status == 0){
				//print_parse_message(PARSE_ERROR, "Invalid declaration found in compound statement", current_line);
				num_errors++;
				return 0;
			}
			//Otherwise we're all good
		} else {
			//Put the token back
			push_back_token(fl, lookahead);

			//In the other case, we must see a statement here
			status = statement(fl);

			//If we failed
			if(status == 0){
				//print_parse_message(PARSE_ERROR, "Invalid statement found in compound statement", current_line);
				num_errors++;
				return 0;
			}
			//Otherwise we're all good
		}

		//Grab the next token to refresh the search
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//We ran off the end, common fail case
	if(lookahead.tok == DONE){
		print_parse_message(PARSE_ERROR, "No closing curly brace given to compound statement", current_line);
		num_errors++;
		return 0;
	}
	
	//When we make it here, we know that we have an R_CURLY in the lookahead
	//Let's check to see if the grouping went properly
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected inside of compound statement", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise everything worked here
	return 1;
}


/**
 * A direct declarator can descend into many different forms
 *
 * BNF Rule: <direct-declarator> ::= <identifier> 
 * 								  | ( <declarator> ) 
 * 								  | <identifier> {[ {constant-expression}? ]}*
 * 								  | <identifier> ( <parameter-type-list>? ) 
 * 								  | <identifier> ( {<identifier>}{, <identifier>}* )
 */
u_int8_t direct_declarator(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	Lexer_item lookahead2;
	u_int8_t status = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We can see a declarator inside of here
	if(lookahead.tok == L_PAREN){
		//Save for later
		push(grouping_stack, lookahead);

		//Now we must see a valid declarator inside of here
		status = declarator(fl);

		//If bad get out
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid declarator found inside of direct declarator", current_line);
			num_errors++;
			return 0;
		}

		//Now we must see a valid closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//No closing paren
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Closing parenthesis expected after declarator", current_line);
			num_errors++;
			return 0;
		}

		//Unmatched grouping operator
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise if we get here we're set
		return 1;

	//The other option, we have an ident
	} else if(lookahead.tok == IDENT){
		//Put it back and call ident
		push_back_token(fl,  lookahead);
		//Call ident just to do handling
		status = identifier(fl);

		if(status == 0){
			print_parse_message(PARSE_ERROR,  "Invalid identifier found in direct declarator", current_line);
			num_errors++;
			return 0;
		}

		//So we see an ident, but certain stuff could come next
		lookahead = get_next_token(fl, &parser_line_num);

		//We have an array subscript here
		if(lookahead.tok == L_BRACKET){
			//We can keep seeing l_brackets here
			while(lookahead.tok == L_BRACKET){
				//Push it onto the stack
				push(grouping_stack, lookahead);

				//Special case, we can see empty ones here
				lookahead2 = get_next_token(fl, &parser_line_num);

				//If we have an empty set here
				if(lookahead2.tok == R_BRACKET){
					//TODO Handle empty brackets
					//Clear this up
					pop(grouping_stack);

					//Keep going through the list
					lookahead = get_next_token(fl, &parser_line_num);

				//Otherwise we need a constant expression here
				} else {
					//Put it back
					push_back_token(fl, lookahead2);

					//See if it works
					status = constant_expression(fl);

					//Fail out if so
					if(status == 0){
						print_parse_message(PARSE_ERROR, "Invalid constant expression in array subscript", current_line);
						return 0;
					}

					//Otherwise, we now have to see an R_BRACKET
					lookahead = get_next_token(fl, &parser_line_num);

					//If we don't see a ]
					if(lookahead.tok != R_BRACKET){
						print_parse_message(PARSE_ERROR, "Right bracket expected to close array subscript", current_line);
						num_errors++;
						return 0;
					}

					//If they don't match
					if(pop(grouping_stack).tok != L_BRACKET){
						print_parse_message(PARSE_ERROR, "Unmatched brackets detected", current_line);
						num_errors++;
						return 0;
					}

					//Otherwise, if we make it all the way here, we will refresh the token
					lookahead = get_next_token(fl, &parser_line_num);
				}
			}

			//If we make it here, lookahead is not an L_Bracket
			//Put it back
			push_back_token(fl, lookahead);
			//Everything worked so success
			return 1;

		//There are two things that could happen if we see an L_PAREN
		} else if(lookahead.tok == L_PAREN){
			//Put this on for matching reasons
			push(grouping_stack, lookahead);
			
			//Handle the special case where it's empty
			lookahead2 = get_next_token(fl, &parser_line_num);

			//If we see this then we're done
			if(lookahead2.tok == R_PAREN){
				//TODO handle accordingly
				//Clean up the stack
				pop(grouping_stack);
				return 1;
			}

			//Otherwise we weren't so lucky
			//Not really needed, but for our sanity
			lookahead = lookahead2;

			//We can see a list of idents here
			if(lookahead.tok == IDENT){
				//TODO handle ident here

				//Grab the next one
				lookahead = get_next_token(fl, &parser_line_num);

				//Handle our list here
				while(lookahead.tok == COMMA){
					//We now need to see another ident
					lookahead = get_next_token(fl, &parser_line_num);
					
					//If it isn't one, that's bad
					if(lookahead.tok != IDENT){
						print_parse_message(PARSE_ERROR, "Identifier expected after comma in identifier list", current_line);
						num_errors++;
						return 0;
					}

					//Otherwise handle the ident TODO

					//Refresh the search
					lookahead = get_next_token(fl, &parser_line_num);
				}

				//We didn't see a comma so bail out
				push_back_token(fl, lookahead);
				//We'll check the parenthesis status at the end
			} else {
				//The associated variable for a parameter, since a parameter is a variable
				//If not, we have to see this
				status = parameter_list(fl);
				
				//If it failed
				if(status == 0){
					//print_parse_message(PARSE_ERROR, "Invalid parameter list in function declarative", current_line);
					num_errors++;
					return 0;
				}
			}

			//Whatever happened, we need to see a closing paren here
			lookahead = get_next_token(fl, &parser_line_num);

			//If we don't see a )
			if(lookahead.tok != R_PAREN){
				print_parse_message(PARSE_ERROR, "Right parenthesis expected", current_line);
				num_errors++;
				return 0;
			}

			//If they don't match
			if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
				num_errors++;
				return 0;
			}

			//If we made it here it worked
			return 1;

		//Regular ident then
		} else {
			//Put it back and get out
			push_back_token(fl, lookahead);
			return 1;
		}

	//If we get here it failed
	} else {
		//print_parse_message(PARSE_ERROR, "Identifier or declarator expected in direct declarator", current_line);
		num_errors++;
		return 0;
	}
}


/**
 * A prime rule that allows us to avoid left recursion
 * 
 * REMEMBER: By the time we arrive here, we've already seen the comma
 *
 * BNF Rule: <initializer-list-prime> ::= , <initializer><initializer-list-prime>
 */
static u_int8_t initializer_list_prime(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid initializer
	status = initializer(fl);

	//Invalid here
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid initializer in initializer list", current_line);
		num_errors++;
		return 0;
	}
	
	//Otherwise we may be able to see a comma and chain the initializer lists
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a comma we know to chain with intializer list prime
	if(lookahead.tok == COMMA){
		return initializer_list_prime(fl);
	} else {
		//Put it back and leave
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An initializer list is a series of initializers chained together
 *
 * BNF Rule: <initializer-list> ::= <initializer><initializer-list-prime>
 */
static u_int8_t initializer_list(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid initializer
	status = initializer(fl);

	//Invalid here
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid initializer in initializer list", current_line);
		num_errors++;
		return 0;
	}
	
	//Otherwise we may be able to see a comma and chain the initializer lists
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a comma we know to chain with intializer list prime
	if(lookahead.tok == COMMA){
		return initializer_list_prime(fl);
	} else {
		//Put it back and leave
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An initializer can descend into a conditional expression or an initializer list
 *
 * BNF Rule: <initializer> ::= <conditional-expression> 
 * 							| { <intializer-list> }
 */
static u_int8_t initializer(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's see what we have in front
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a left curly, we know that we have an intializer list
	if(lookahead.tok == L_CURLY){
		//Push to stack for checking
		push(grouping_stack, lookahead);

		//Now we just see a valid initializer list
		u_int8_t status = initializer_list(fl);

		//Fail out here
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid initializer list in initializer", current_line);
			num_errors++;
			return 0;
		}
		
		//Now we have to see a closing curly
		lookahead = get_next_token(fl, &parser_line_num);

		//If we don't see it
		if(lookahead.tok != R_CURLY){
			print_parse_message(PARSE_ERROR, "Closing curly brace expected after initializer list", current_line);
			num_errors++;
			return 0;
		}
		
		//Unmatched curlies here
		if(pop(grouping_stack).tok != L_CURLY){
			print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked so we can get out
		return 1;

	//If we didn't see the curly, we must see a conditional expression
	} else {
		//Put the token back
		push_back_token(fl, lookahead);

		//Must work here
		status = conditional_expression(fl);

		//Fail out if we get here
		if(status == 0){
			//print_parse_message(PARSE_ERROR, "Invalid conditional expression found in initializer", current_line);
			num_errors++;
			return 0;
		}
		
		//Otherwise it worked, so return 1
		return 1;
	}
}


/**
 * A declarator has an optional pointer type and is followed by a direct declarator
 *
 * BNF Rule: <declarator> ::= {<pointer>}? <direct-declarator>
 */
static u_int8_t declarator(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	u_int8_t status = 0;

	//We can see pointers here
	status = pointer(fl);

	//If we see any pointers, handle them accordingly TODO
	
	//Now we must see a valid direct declarator
	status = direct_declarator(fl);
	
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid direct declarator found in declarator", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise we're all set so return 1
	return 1;
}


/**
 * A declaration is the other main kind of block that we can see other than functions
 *
 * BNF Rule: <declaration> ::= declare {constant}? <storage-class-specifier>? <type-specifier> {<pointer>}? <direct-declarator>; 
 * 							 | let {constant}? <storage-class-specifier>? <type-specifier> {<pointer>}? <direct-declarator := <intializer>;
 *                           | define <enumerated-definer> {as <ident>}?;
 *                           | define <structure-definer> {as <ident>}?;
 *                           | alias <type-specifier> {<pointer>}? as <ident>;
 */
static u_int8_t declaration(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//Generic status currently
	u_int8_t status = 0;
	//What is the storage class of our variable?
	STORAGE_CLASS_T storage_class = STORAGE_CLASS_NORMAL;
	//Keep track if it's a const or not
	u_int8_t is_constant = 0;
	//The type that we have
	basic_type_t type;
	//The var name
	char var_name[100];
	//Wipe it
	memset(var_name, 0, 100*sizeof(char));

	//Grab the token
	lookahead = get_next_token(fl, &parser_line_num);

	//Handle declaration
	if(lookahead.tok == DECLARE){
		//We can optionally see the constant keyword here
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If it's constant we'll simply set the flag
		if(lookahead.tok == CONSTANT){
			is_constant = 1;
			//Refresh lookahead
			lookahead = get_next_token(fl, &parser_line_num);
		}
		//Otherwise we'll keep the same token for our uses

		//Now we can optionally see storage class specifiers here
		//If we see one here
		if(lookahead.tok == STATIC){
			storage_class = STORAGE_CLASS_STATIC;
		//Would make no sense so fail out
		} else if(lookahead.tok == EXTERNAL){
			//TODO
			print_parse_message(PARSE_ERROR, "External variables are not yet supported", current_line);
			num_errors++;
			return 0;
		} else if(lookahead.tok == REGISTER){
			storage_class = STORAGE_CLASS_REGISTER;
		} else {
			//Otherwise, put the token back and get out
			push_back_token(fl, lookahead);
			storage_class = STORAGE_CLASS_NORMAL;
		}
	
		//Now we must see a valid type specifier
		status = type_specifier(fl);

		//fail case
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid type given to declaration", current_line);
			num_errors++;
			return 0;
		}

		//Now we can optionally see several pointers
		//Just let this do its thing
		pointer(fl);

		//Then we must see a direct declarator
		status = direct_declarator(fl);

		//fail case
		if(status == 0){
			num_errors++;
			return 0;
		}

		//Let's check if we can actually find it
		symtab_variable_record_t* found_var = lookup_variable(variable_symtab, current_ident->lexeme);

		//Can we grab it
		if(found_var != NULL){
			print_parse_message(PARSE_ERROR, "Illegal variable redefinition. First defined here:", current_line);
			print_variable_name(found_var);
			num_errors++;
			return 0;
		}

		//Ollie language also does not allow duplicate function names
		symtab_function_record_t* found_func = lookup_function(function_symtab, current_ident->lexeme);

		//Can we grab it
		if(found_func != NULL){
			print_parse_message(PARSE_ERROR, "Variables may not share the same names as functions. First defined here:", current_line);
			print_function_name(found_func);
			num_errors++;
			return 0;
		}

		//Ollie language also does not allow duplicated type names
		symtab_type_record_t* found_type = lookup_type(type_symtab, current_ident->lexeme);

		//Can we grab it
		if(found_type != NULL){
			print_parse_message(PARSE_ERROR, "Variables may not share the same names as types. First defined here:", current_line);
			print_type_name(found_type);
			num_errors++;
			return 0;
		}
		//Otherwise we're in the clear here

		//Otherwise all should have gone well here, so we can construct our declaration
		symtab_variable_record_t* var = create_variable_record(current_ident->lexeme, storage_class);
		//It was not initialized
		var->initialized = 0;
		//What's the type
		var->type = active_type;
		//The current line
		var->line_number = current_line;
		//Not a function param
		var->is_function_paramater = 0;
		//Was made using DECLARE(0)
		var->declare_or_let = 0;
		
		//Store for our uses
		insert_variable(variable_symtab, var);

		//Now once we make it here, we need to see a SEMICOL
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected at the end of declaration", current_line);
			//Free the active type condition
			num_errors++;
			return 0;
		}

		//Once we make it here, we know it worked
		active_type = NULL;
		free(current_ident);
		current_ident = NULL;

		return 1;

	//Handle declaration + assignment
	} else if(lookahead.tok == LET){
		//We can optionally see the constant keyword here
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If it's constant we'll simply set the flag
		if(lookahead.tok == CONSTANT){
			is_constant = 1;
			//Refresh lookahead
			lookahead = get_next_token(fl, &parser_line_num);
		}
		//Otherwise we'll keep the same token for our uses

		//Now we can optionally see storage class specifiers here
		//If we see one here
		if(lookahead.tok == STATIC){
			storage_class = STORAGE_CLASS_STATIC;
		//Would make no sense so fail out
		} else if(lookahead.tok == EXTERNAL){
			//TODO
			print_parse_message(PARSE_ERROR, "External variables are not yet supported", current_line);
			num_errors++;
			return 0;
		} else if(lookahead.tok == REGISTER){
			storage_class = STORAGE_CLASS_REGISTER;
		} else {
			//Otherwise, put the token back and get out
			push_back_token(fl, lookahead);
			storage_class = STORAGE_CLASS_NORMAL;
		}

		//Now we must see a valid type specifier
		status = type_specifier(fl);

		//fail case
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid type given to declaration", current_line);
			num_errors++;
			return 0;
		}

		//Now we can optionally see several pointers
		//Just let this do its thing
		pointer(fl);

		//Then we must see a direct declarator
		status = direct_declarator(fl);

		//fail case
		if(status == 0){
			num_errors++;
			return 0;
		}

		//Let's check if we can actually find it
		symtab_variable_record_t* found = lookup_variable(variable_symtab, current_ident->lexeme);

		if(found != NULL){
			print_parse_message(PARSE_ERROR, "Illegal variable redefinition. First defined here:", current_line);
			print_variable_name(found);
			num_errors++;
			return 0;
		}

		//Ollie language also does not allow duplicate function names
		symtab_function_record_t* found_func = lookup_function(function_symtab, current_ident->lexeme);

		//Can we grab it
		if(found_func != NULL){
			print_parse_message(PARSE_ERROR, "Variables may not share the same names as functions. First defined here:", current_line);
			print_function_name(found_func);
			num_errors++;
			return 0;
		}

		//Ollie language also does not allow duplicated type names
		symtab_type_record_t* found_type = lookup_type(type_symtab, current_ident->lexeme);

		//Can we grab it
		if(found_type != NULL){
			print_parse_message(PARSE_ERROR, "Variables may not share the same names as types. First defined here:", current_line);
			print_type_name(found_type);
			num_errors++;
			return 0;
		}
		//Otherwise we're in the clear here

		//Otherwise all should have gone well here, so we can construct our declaration
		symtab_variable_record_t* var = create_variable_record(current_ident->lexeme, storage_class);
		//It should be initialized in this case
		var->initialized = 1;
		//What's the type
		var->type = active_type;
		//The current line
		var->line_number = current_line;
		//Not a function param
		var->is_function_paramater = 0;
		//Was made using LET(1) 
		var->declare_or_let = 1;
		
		//Store for our uses
		insert_variable(variable_symtab, var);

		//Now we need to see a valid := initializer;
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out
		if(lookahead.tok != COLONEQ){
			print_parse_message(PARSE_ERROR, "Assignment operator(:=) expected in let statement", current_line);
			num_errors++;
			return 0;
		}
		
		//Now we have to see a valid initializer
		status = initializer(fl);

		//Fail out
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid initialization in let statement", current_line);
			num_errors++;
			return 0;
		}

		//TODO NEED MANY MORE TYPE CHECKS HERE

		//Now once we make it here, we need to see a SEMICOL
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected at the end of declaration", current_line);
			//Free the active type condition
			num_errors++;
			return 0;
		}

		//Once we make it here, we know it worked
		free(current_ident);
		active_type = NULL;
		current_ident = NULL;

		return 1;

	//Handle type definition. This works for enum types and structure types 
	} else if(lookahead.tok == DEFINE){
		//Now let's see what kind of definition that we have
		lookahead = get_next_token(fl, &parser_line_num);
		
		//Enumerated type
		if(lookahead.tok == ENUMERATED){
			//Go through an do an enumeration defintion
			status = enumeration_definer(fl);
			
			//Fail case
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid enumeration defintion given",  current_line);
				num_errors++;
				return 0;
			}

			//Otherwise we'll add into the symtable
			insert_type(type_symtab, create_type_record(active_type));

		//Constructed type
		} else if(lookahead.tok == CONSTRUCT){
			//Go through and do a construct definition
			status = construct_definer(fl);
			
			//Fail case
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid construct definition given",  current_line);
				num_errors++;
				return 0;
			}

			//Otherwise we'll add into the symtable
			insert_type(type_symtab, create_type_record(active_type));
		}

		//We must see a semicol to round things out
		lookahead = get_next_token(fl, &parser_line_num);

		//If we see the as keyword, we are doing a type alias
		//Ollie language supports type aliases immediately upon definition
		if(lookahead.tok == AS){
			//We now must see a valid IDENT
			status = identifier(fl);

			//If we don't see that, we're out of here
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
				num_errors++;
				return 0;
			}
			//Otherwise it worked, our ident is now stored in current_ident

			//Let's do some checks to ensure that we don't have duplicate names
			//Let's check if we can actually find it
			symtab_variable_record_t* found = lookup_variable(variable_symtab, current_ident->lexeme);

			if(found != NULL){
				print_parse_message(PARSE_ERROR, "Aliases and variables may not share names. First defined here:", current_line);
				print_variable_name(found);
				num_errors++;
				return 0;
			}

			//Ollie language also does not allow duplicate function names
			symtab_function_record_t* found_func = lookup_function(function_symtab, current_ident->lexeme);

			//Can we grab it
			if(found_func != NULL){
				print_parse_message(PARSE_ERROR, "Aliases may not share the same names as functions. First defined here:", current_line);
				print_function_name(found_func);
				num_errors++;
				return 0;
			}

			//Ollie language also does not allow duplicated type names
			symtab_type_record_t* found_type = lookup_type(type_symtab, current_ident->lexeme);

			//Can we grab it
			if(found_type != NULL){
				print_parse_message(PARSE_ERROR, "Aliases may not share the same names as previously defined types/aliases. First defined here:", current_line);
				print_type_name(found_type);
				num_errors++;
				return 0;
			}
			//Otherwise we're in the clear here
			
			//Store this for now
			generic_type_t* temp = active_type;

			//Create the aliased type
			active_type = create_aliased_type(current_ident->lexeme, temp, parser_line_num);

			//Put into the symtab now
			insert_type(type_symtab, create_type_record(active_type));

		} else {
			//Put it back, no alias
			push_back_token(fl, lookahead);
		}
	
		//Finally we need to see a semicol here
		lookahead = get_next_token(fl, &parser_line_num);

		//Automatic fail case
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected at the end of definition statement", parser_line_num);
			num_errors++;
			return 0;
		}

		//Otherwise it worked so we can leave
		return 1;

	//Alias statement
	} else if(lookahead.tok == ALIAS){

		return 0;

	//We had some failure here
	} else {
		print_parse_message(PARSE_ERROR, "Declare, let, define or alias keyword expected in declaration block", current_line);
		num_errors++;
		return 0;
	}
}


/**
 * A function specifier has two options, the rule merely exists for AST integration
 *
 * ALWAYS A CHILD
 */
static u_int8_t function_specifier(FILE* fl, generic_ast_node_t* parent_node){

	//We need to see static or external keywords here
	Lexer_item lookahead = get_next_token(fl, &parser_line_num);
	
	//IF we got here, we need to see static or external
	if(lookahead.tok == STATIC || lookahead.tok == EXTERNAL){
		//Create a new node
		generic_ast_node_t* node = ast_node_alloc(AST_NODE_CLASS_FUNC_SPECIFIER);
	
		//Assign the token here and attach it to the tree
		((func_specifier_ast_node_t*)(node->node))->funcion_storage_class_tok = lookahead.tok;

		//Assign these for ease of use later in the parse tree
		if(lookahead.tok == STATIC){
			((func_specifier_ast_node_t*)(node->node))->function_storage_class = STORAGE_CLASS_STATIC;
		} else {
			((func_specifier_ast_node_t*)(node->node))->function_storage_class = STORAGE_CLASS_EXTERNAL;
		}

		//This node is always a child of a parent node. Accordingly so, we'll use the
		//helper function to attach it
		add_child_node(parent_node, node);

		//Succeeded
		return 1;

	//Fail case here
	} else {
		print_parse_message(PARSE_ERROR, "STATIC or EXTERNAL keywords expected after colon in function declaration", parser_line_num);
		num_errors++;
		return 0;
	}
}


/**
 * Handle the case where we declare a function
 *
 * BNF Rule: <function-definition> ::= func {<function-specifier>}? <identifer> ({<parameter-list>}?) -> {constant}? <type-specifier> <compound-statement>
 *
 * REMEMBER: By the time we get here, we've already seen the func keyword
 */
static u_int8_t function_definition(FILE* fl, generic_ast_node_t* parent_node){
	//This will be used for error printing
	char info[2000];
	
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//The status tracker
	u_int8_t status = 0;
	//Lookahead token
	Lexer_item lookahead;

	//What is the function's storage class? Normal by default
	STORAGE_CLASS_T storage_class;
	
	//The function record -- not initialized until IDENT
	symtab_function_record_t* function_record;

	//We also have the AST function node, this will be intialized immediately
	//It also requires a symtab record of the function, but this will be assigned
	//later once we have it
	generic_ast_node_t* function_node = ast_node_alloc(AST_NODE_CLASS_FUNC_DEF);

	//The function node will be a child of the parent, so we'll add it in as such
	add_child_node(parent_node, function_node);

	//REMEMBER: by the time we get here, we've already seen and consumed "FUNC"
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We've seen the option function specifier
	if(lookahead.tok == COLON){
		//If we see this, we must then see a valid function specifier
		status = function_specifier(fl, function_node);

		//Invalid function specifier -- error out
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid function specifier seen after \":\"",  current_line);
			return 0;
		}

		//Refresh current line
		current_line = parser_line_num;

		//Also stash this for later use
		storage_class = ((func_specifier_ast_node_t*)(function_node->first_child->node))->function_storage_class;

	//Otherwise it's a plain function so put the token back
	} else {
		//Otherwise put the token back in the stream
		push_back_token(fl, lookahead);
		//Normal storage class
		storage_class = STORAGE_CLASS_NORMAL;
	}

	//Now we must see an identifer
	status = identifier(fl, function_node);

	//We have no identifier, so we must quit
	if(status == 0){
		print_parse_message(PARSE_ERROR, "No valid identifier found for function", current_line);
		num_errors++;
		return 0;
	}

	//We'll deal with assembling the function record later on
	
	//Now we need to see a valid parentheis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't find it, no point in going further
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise, we'll push this onto the list to check for later
	push(grouping_stack, lookahead);
	
	//Now we must ensure that we see a valid parameter list. It is important to note that
	//parameter lists can be empty, but whatever we have here we'll have to add in
	//Parameter list parent is the function node
	status = parameter_list(fl, function_node);

	//We have a bad parameter list
	if(status == 0){
		print_parse_message(PARSE_ERROR, "No valid parameter list found for function", current_line);
		num_errors++;
		return 0;
	}
	
	//Now we need to see a valid closing parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't have an R_Paren that's an issue
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected", current_line);
		num_errors++;
		return 0;
	}
	
	//If this happens, then we have some unmatched parenthesis
	if(pop(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis found", current_line);
		num_errors++;
		return 0;
	}

	//We will handle all type checking at the end of the function, after we've parsed the top level declaration

	//Let's see if we have duplicate function names
	symtab_function_record_t* func_record = lookup_function(function_symtab, current_ident->lexeme);

	//If we can find it we have a duplicate function
	if(func_record != NULL && func_record->defined == 1){
		print_parse_message(PARSE_ERROR, "Illegal redefinition of function. First defined here: ", current_line);
		print_function_name(func_record);
		num_errors++;
		return 0;
	}

	//Let's see if we're trying to redefine variable names
	symtab_variable_record_t* var_record = lookup_variable(variable_symtab, current_ident->lexeme);

	//If we can find it we have a duplicate function
	if(var_record != NULL){
		print_parse_message(PARSE_ERROR, "Functions and variables may not share names. First defined here:", current_line);
		print_variable_name(var_record);
		num_errors++;
		return 0;
	}

	//Let's see if we're trying to redefine a custom type name
	symtab_type_record_t* type_record = lookup_type(type_symtab, current_ident->lexeme);

	//If we can find it we have a duplicate function
	if(type_record != NULL){
		print_parse_message(PARSE_ERROR, "Functions and types may not share the same name. First defined here:", current_line);
		print_type_name(type_record);
		num_errors++;
		return 0;
	}

	//Officially make the function record
	function_record = create_function_record(current_ident->lexeme, storage_class);
	//Store the line number too
	function_record->line_number = current_line;

	//Now that we're done using these we can free them
	free(current_ident);
	current_ident = NULL;

	//Set these equal here
	current_function = function_record;

	//Insert this into the function symtab
	insert_function(function_symtab, function_record);

	//Past the point where we've seen the param_list
arrow_ident:
	//Grab the next one
	lookahead = get_next_token(fl, &parser_line_num);

	//We absolutely must see an arrow here
	if(lookahead.tok != ARROW){
		print_parse_message(PARSE_ERROR, "Arrow expected after function declaration", current_line);
		num_errors++;
		return 0;
	}

	//After the arrow we must see a valid type specifier
	status = type_specifier(fl);

	//If it failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid return type given to function", current_line);
		num_errors++;
		return 0;
	}

	//We can also see pointers here--status is irrelevant this is just for type information
	pointer(fl);

	//We'll store this as the function return type
	function_record->return_type = active_type;

	//Now we must see a compound statement
	status = compound_statement(fl);

	//If we fail here
	if(status == 0){
		//print_parse_message(PARSE_ERROR, "Invalid compound statement in function", current_line);
		num_errors++;
		return 0;
	}

	//Finalize the variable scope initialized in the param list
	finalize_variable_scope(variable_symtab);

	//Set this to null to avoid confusion
	current_function = NULL;

	//Set to NULL for the next time around
	active_type = NULL;

	//All went well if we make it here
	return 1;
}


/**
 * Here we can either have a function definition or a declaration
 *
 * The AST will not be modified in this function, as these are pass through
 * rules that have no nonterminals
 *
 * <declaration-partition>::= <function-definition>
 *                        	| <declaration>
 */
static u_int8_t declaration_partition(FILE* fl, generic_ast_node_t* parent_node){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	u_int8_t status;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//We know that we have a function here
	if(lookahead.tok == FUNC){
		//Otherwise our status is just whatever the function returns
		status = function_definition(fl, parent_node);

		//Something failed
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid function definition", current_line);
			return 0;
		}

	} else {
		//Push it back
		push_back_token(fl, lookahead);
		//Otherwise, the only other option is a declaration
		status = declaration(fl);

		//Something failed
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid top-level declaration", current_line);
			return 0;
		}
	}
	
	//If we get here it worked
	return 1;
}


/**
 * Here is our entry point
 *
 * BNF Rule: <program>::= {<declaration-partition>}*
 */
static u_int8_t program(FILE* fl){
	//Freeze the line number
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We first symbolically "see" the START token. The start token
	//is the lexer symbol that the top level node holds
	Lexer_item start;

	//We really only care about the tok here
	start.tok = START;
	
	//Create the ROOT of the tree
	ast_root = ast_node_alloc(AST_NODE_CLASS_PROG);
	//Assign the lexer item to it for completeness
	((prog_ast_node_t*)(ast_root->node))->lex = start;

	//As long as we aren't done
	while((lookahead = get_next_token(fl, &parser_line_num)).tok != DONE){
		//Put the token back
		push_back_token(fl, lookahead);

		//Pass along and let the rest handle
		status = declaration_partition(fl, ast_root);
		
		//No need for error printing here, should be handled by bottom level prog
		if(status == 0){
			return 0;
		}
	}

	//All went well if we get here
	return 1;
}


/**
 * Entry point for our parser. Everything beyond this point will be called in a recursive-descent fashion through
 * static methods
*/
u_int8_t parse(FILE* fl){
	u_int8_t status = 0;
	num_errors = 0;
	double time_spent;

	//Start the timer
	clock_t begin = clock();

	//Initialize all of our symtabs
	function_symtab = initialize_function_symtab();
	variable_symtab = initialize_variable_symtab();
	type_symtab = initialize_type_symtab();

	//For the type and variable symtabs, their scope needs to be initialized before
	//anything else happens
	
	//Initialize the variable scope
	initialize_variable_scope(variable_symtab);
	//Global variable scope here
	initialize_type_scope(type_symtab);

	//Add all basic types into the type symtab
	add_all_basic_types(type_symtab);

	//Also create a stack for our matching uses(curlies, parens, etc.)
	grouping_stack = create_stack();

	//Global entry/run point
	status = program(fl);

	//Timer end
	clock_t end = clock();
	//Crude time calculation
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

	//If we failed
	if(status == 0){
		char info[500];
		sprintf(info, "Parsing failed with %d errors in %.8f seconds", num_errors, time_spent);
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", parser_line_num);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
	} else {
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", parser_line_num);
		printf("Parsing succeeded in %.8f seconds\n", time_spent);
		printf("=======================================================================\n\n");
	}
	
	//Clean these both up for memory safety
	destroy_stack(grouping_stack);
	//Deallocate all symtabs
	destroy_function_symtab(function_symtab);
	destroy_variable_symtab(variable_symtab);
	destroy_type_symtab(type_symtab);
	
	//Deallocate the AST
	deallocate_ast(ast_root);

	return status;
}
