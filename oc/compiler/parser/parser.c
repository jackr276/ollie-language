/**
 * The parser for Ollie-Lang
 *
 * GOAL: The goal of the parser is to determine if the input program is a syntatically valid sentence in the language.
 * This is done via recursive-descent in our case. As the 
*/

#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

//Our global symbol table
symtab_t* symtab;
//Our stack for storing variables, etc
stack_t* variable_stack;
stack_t* grouping_stack;
//The number of errors
u_int16_t num_errors = 0;
//The current parser line number
u_int16_t parser_line_num = 0;

//Function prototypes are predeclared here as needed to avoid excessive restructuring of program
static u_int8_t cast_expression(FILE* fl);
static u_int8_t assignment_expression(FILE* fl);
static u_int8_t conditional_expression(FILE* fl);
static u_int8_t unary_expression(FILE* fl);

/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_t* parse_message){
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	printf("[LINE %d: PARSER %s]: %s\n", parse_message->line_num, type[parse_message->message], parse_message->info);
}


/**
 * Do we have an identifier or not?
 */
static u_int8_t identifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	parse_message_t message;
	char info[500];
	
	//If we can't find it that's bad
	if(l.tok != IDENT){
		message.message = PARSE_ERROR;
		sprintf(info, "String %s is not a valid identifier", l.lexeme);
		message.line_num = l.line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//We'll push this ident onto the stack and let whoever called(function/variable etc.) deal with it
	//We have no need to search the symtable in this function because we are unable to context-sensitive
	//analysis here
	push(variable_stack, l);
	return 1;
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
	parse_message_t message;
	Lexer_item l;
	
	//We should see one of the 4 constants here
	l = get_next_token(fl, &parser_line_num);

	//Do this for now, later on we'll need symtable integration
	if(l.tok == INT_CONST || l.tok == STR_CONST || l.tok == CHAR_CONST || l.tok == FLOAT_CONST ){
		return 1;
	}

	//Otherwise here, we have an error
	message.message = PARSE_ERROR;
	message.info = "Invalid constant found";
	message.line_num = parser_line_num;
	print_parse_message(&message);
	num_errors++;
	return 0;
}


static u_int8_t declaration(FILE* fl){
	return 0;
}

static u_int8_t type_name(FILE* fl){

}


/**
 * A prime rule that allows for comma chaining and avoids right recursion
 *
 * REMEMBER: by the time that we've gotten here, we've already seen a comma
 *
 * BNF Rule: <expression_prime> ::= , <assignment-expression><expression_prime>
 */
static u_int8_t expression_prime(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status;

	//We must first see a valid assignment-expression
	status = assignment_expression(fl);
	
	//It failed
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid assignment expression found in expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status;

	//We must first see a valid assignment-expression
	status = assignment_expression(fl);
	
	//It failed
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid assignment expression found in expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
 * A primary expression is, in a way, the termination of our expression chain. However, it can be used 
 * to chain back up to an expression in general using () as an enclosure
 *
 * BNF Rule: <primary-expression> ::= <identifier>
 * 									| <constant> 
 * 									| (<expression>)
 */
static u_int8_t primary_expression(FILE* fl){
	parse_message_t message;
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
			message.message = PARSE_ERROR;
			message.info = "Invalid identifier found in primary expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
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
			message.message = PARSE_ERROR;
			message.info = "Invalid constant found in primary expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
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
			message.message = PARSE_ERROR;
			message.info = "Invalid expression found in primary expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//We must now also see an R_PAREN, and ensure that we have matching on the stack
		lookahead = get_next_token(fl, &parser_line_num);

		//If it isn't a right parenthesis
		if(lookahead.tok != R_PAREN){
			message.message = PARSE_ERROR;
			message.info = "Right parenthesis expected after expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		//Make sure we match
		} else if(pop(grouping_stack).tok != L_PAREN){
			message.message = PARSE_ERROR;
			message.info = "Unmatched parenthesis detected";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}
		
		//Otherwise we're all set
		return 1;
	} else {
		message.message = PARSE_ERROR;
		char info[500];
		memset(info, 0, 500 * sizeof(char));
		sprintf(info, "Invalid token with lexeme %s found in primary expression", lookahead.lexeme); 
		message.info = info;
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
}


/**
 * An assignment expression can decay into a conditional expression or it
 * can actually do assigning. There is no chaining in Ollie language of assignments
 *
 * BNF Rule: <assignment-expression> ::= <conditional-expression> 
 * 									   | let <unary-expression> := <conditional-expression>
 */
static u_int8_t assignment_expression(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//We've seen the LET keyword
	if(lookahead.tok == LET){
		//Since we've seen this, we now need to see a valid unary expression
		status = unary_expression(fl);

		//We have a bad one
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid unary expression found in assignment expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Now we must see the := assignment operator
		lookahead = get_next_token(fl, &parser_line_num);

		//We have a bad one
		if(lookahead.tok != COLONEQ){
			message.message = PARSE_ERROR;
			message.info = "Assignment operator := expected after unary expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Otherwise it worked just fine
		//Now we must see an assignment expression again
		status = assignment_expression(fl);

		//We have a bad one
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid conditional expression found in assignment expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
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
			message.message = PARSE_ERROR;
			message.info = "Invalid conditional expression found in postfix expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
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
 * 									| <primary-expression> ( {assignment-expression}* ) 
 * 									| <primary-expression> ++ 
 * 									| <primary-expression> --
 */ 
static u_int8_t postfix_expression(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid primary expression no matter what
	status = primary_expression(fl);

	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid primary expression found in postfix expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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

		//If we see a left paren, we are looking at an assignment expression
		case L_PAREN:
			//Push to the stack for later
			push(grouping_stack, lookahead);

			//Now we can see 0 or many assignment expressions
			lookahead = get_next_token(fl, &parser_line_num);

			//As long as we don't see the enclosing right parenthesis
			while(lookahead.tok != R_PAREN){
				//Put it back
				push_back_token(fl, lookahead);
				
				//We now need to see a valid assignment expression
				status = assignment_expression(fl);
				
				//Refresh this for the next search
				lookahead = get_next_token(fl, &parser_line_num);
			}
			
			//Once we break out here, in theory our token will be a right paren
			//Just to double check
			if(lookahead.tok != R_PAREN){
				message.message = PARSE_ERROR;
				message.info = "Right parenthesis expected after primary expression";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			//Some unmatched parenthesis here
			} else if(pop(grouping_stack).tok != L_PAREN){
				message.message = PARSE_ERROR;
				message.info = "Unmatched parenthesis detected";
				message.line_num = parser_line_num;
				print_parse_message(&message);
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
					message.message = PARSE_ERROR;
					message.info = "Invalid expression in primary expression index";
					message.line_num = parser_line_num;
					print_parse_message(&message);
					num_errors++;
					return 0;
				}

				//Now we have to see a valid right bracket
				lookahead = get_next_token(fl, &parser_line_num);

				//Just to double check
				if(lookahead.tok != R_BRACKET){
					message.message = PARSE_ERROR;
					message.info = "Right bracket expected after primary expression index";
					message.line_num = parser_line_num;
					print_parse_message(&message);
					num_errors++;
					return 0;
				//Or we have some unmatched grouping operator
				} else if(pop(grouping_stack).tok != L_BRACKET){
					message.message = PARSE_ERROR;
					message.info = "Unmatched bracket deteced";
					message.line_num = parser_line_num;
					print_parse_message(&message);
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
	parse_message_t message;
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
				message.message = PARSE_ERROR;
				message.info = "Invalid unary expression following preincrement/predecrement";
				message.line_num = parser_line_num;
				print_parse_message(&message);
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
				message.message = PARSE_ERROR;
				message.info = "Left parenthesis expected after size keyword";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			}
			
			//Push it onto the stack
			push(grouping_stack, lookahead);
			
			//Now we must see a valid unary expression
			status = unary_expression(fl);

			//If it was bad
			if(status == 0){
				message.message = PARSE_ERROR;
				message.info = "Invalid unary expression given to size operator";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			}
		
			//If we get here though we know it worked
			//Now we must see a valid closing parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			//If this is an R_PAREN
			if(lookahead.tok != R_PAREN) {
				message.message = PARSE_ERROR;
				message.info = "Right parenthesis expected after unary expression";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			//Otherwise if it wasn't matched right
			} else if(pop(grouping_stack).tok != L_PAREN){
				message.message = PARSE_ERROR;
				message.info = "Unmatched parenthesis detected";
				message.line_num = parser_line_num;
				print_parse_message(&message);
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
				message.message = PARSE_ERROR;
				message.info = "Left parenthesis expected after typesize keyword";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			}
			
			//Push it onto the stack
			push(grouping_stack, lookahead);
			
			//Now we must see a valid type name 
			status = type_name(fl);

			//If it was bad
			if(status == 0){
				message.message = PARSE_ERROR;
				message.info = "Invalid type name given to typesize operator";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			}
		
			//If we get here though we know it worked
			//Now we must see a valid closing parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			//If this is an R_PAREN
			if(lookahead.tok != R_PAREN) {
				message.message = PARSE_ERROR;
				message.info = "Right parenthesis expected after type name";
				message.line_num = parser_line_num;
				print_parse_message(&message);
				num_errors++;
				return 0;
			//Otherwise if it wasn't matched right
			} else if(pop(grouping_stack).tok != L_PAREN){
				message.message = PARSE_ERROR;
				message.info = "Unmatched parenthesis detected";
				message.line_num = parser_line_num;
				print_parse_message(&message);
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
		case CONDITIONAL_DEREF:
		case B_NOT:
		case L_NOT:
			//No matter what we see here, we will have to see a valid cast expression after it
			status = cast_expression(fl);

			//If it was bad
			if(status == 0){
				message.message = PARSE_ERROR;
				message.info = "Invalid cast expression following unary operator";
				message.line_num = parser_line_num;
				print_parse_message(&message);
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
				message.message = PARSE_ERROR;
				message.info = "Invalid postfix expression inside of unary expression";
				message.line_num = parser_line_num;
				print_parse_message(&message);
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
 * 						    	| ( <type-name> ) <unary-expression>
 */
static u_int8_t cast_expression(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Let's see if we have a parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have an L_PAREN, we'll push it to the stack
	if(lookahead.tok == L_PAREN){
		//Push this on for later
		push(grouping_stack, lookahead);
		
		//We now must see a valid type name
		status = type_name(fl);
		
		//TODO symtab integration
		
		//If it was bad
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid type name found in cast expression";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}
		
		//Otherwise we're good to keep going
		//We now must see a valid R_PAREN
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If this is an R_PAREN
		if(lookahead.tok != R_PAREN) {
			message.message = PARSE_ERROR;
			message.info = "Right parenthesis expected after type name";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		} else if(pop(grouping_stack).tok != L_PAREN){
			message.message = PARSE_ERROR;
			message.info = "Unmatched parenthesis detected";
			message.line_num = parser_line_num;
			print_parse_message(&message);
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
		message.message = PARSE_ERROR;
		message.info = "Invalid unary expression found in cast expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid cast expression
	status = cast_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid cast expression found in multiplicative expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid cast expression
	status = cast_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid cast expression found in multiplicative expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//First we must see a valid multiplicative expression
	status = multiplicative_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid multiplicative expression found in additive expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see * or / here
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a + or a - we can make a recursive call
	if(lookahead.tok == MINUS || lookahead.tok == STAR){
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//First we must see a valid multiplicative expression
	status = multiplicative_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid multiplicative expression found in additive expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid additive expression
	status = additive_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid additive expression found in shift expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
		message.message = PARSE_ERROR;
		message.info = "Invalid additive expression found in shift expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid shift expression
	status = shift_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid shift expression found in relational expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
		message.message = PARSE_ERROR;
		message.info = "Invalid shift expression found in relational expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//If we get to here then we're all good
	return 1;
}


/**
 * A prime rule that allows us to avoid left recursion
 *
 * REMEMBER: By the time that we've gotten here, we will have already seen == or !=
 *
 * BNF Rule: <equality-expression-prime> ::= ==<relational-expression><equality-expression-prime> 
 *										   | !=<relational-expression><equality-expression-prime>
 */
static u_int8_t equality_expression_prime(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid relational-expression
	status = relational_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid relational expression found in equality expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a == or a != we can make a recursive call
	if(lookahead.tok == D_EQUALS || lookahead.tok == NOT_EQUALS){
		return equality_expression_prime(fl);
	} else {
		//Otherwise we need to put it back and get out
		push_back_token(fl, lookahead);
		return 1;
	}
}


/**
 * An equality expression can be chained and descends into a relational expression 
 *
 * BNF Rule: <equality-expression> ::= <relational-expression> 
 * 									 | <relational-expression><equality-expression-prime>
 */
static u_int8_t equality_expression(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid relational-expression
	status = relational_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid relational expression found in equality expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Otherwise, we may be able to see the double && here to chain
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a == or a != we can make a recursive call
	if(lookahead.tok == D_EQUALS || lookahead.tok == NOT_EQUALS){
		return equality_expression_prime(fl);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid equality-expression
	status = equality_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid equality expression found in and expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid equality-expression
	status = equality_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid equality expression found in and expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid and-expression
	status = and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid and expression found in exclusive or expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid and-expression
	status = and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid and expression found in exclusive or expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid exclusive or expression
	status = exclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid exclusive or expression found in inclusive or expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = exclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid exclusive or expression found in inclusive or expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = inclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid inclusive or expression found in logical and expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = inclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid inclusive or expression found in logical and expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status;

	//We now must see a valid logical and expression
	status = logical_and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid logical and expression found in logical or expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We first must see a valid logical and expression
	status = logical_and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid logical and expression found in logical or expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	//Pass through to the conditional expression
	u_int8_t status = conditional_expression(fl);

	//Something failed
	if(status == 0){
		//Otherwise we've failed completely
		message.message = PARSE_ERROR;
		message.info = "Invalid logical or expression found in conditional expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
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
	parse_message_t message;
	//Pass through to the conditional expression
	u_int8_t status = conditional_expression(fl);

	//Something failed
	if(status == 0){
		//Otherwise we've failed completely
		message.message = PARSE_ERROR;
		message.info = "Invalid conditional expression found in constant expression";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Otherwise we're all set
	return 1;
}


u_int8_t direct_declarator(FILE* fl){
	return 0;
}

u_int8_t declarator(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t structure_declarator(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t structure_declarator_list(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t specifier_qualifier(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t structure_declaration(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t structure_specifier(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t type(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t pointer(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


/**
 * For an enumerator, we can see an ident or an assigned ident
 *
 * BNF Rule: <enumerator> ::= <identifier> 
 * 			           	  | <identifier> := <constant-expression>
 */
u_int8_t enumerator(FILE* fl){
	parse_message_t message;
	Lexer_item lookahead;
	u_int8_t status;

	//We must see a valid identifier here
	status = identifier(fl);

	//Get out if bad
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid identifier in enumerator";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Let's see what's up ahead
	lookahead = get_next_token(fl, &parser_line_num);

	//We're now seeing a constant expression here
	if(lookahead.tok == COLONEQ){
		//We now must see a valid constant expression
		status = constant_expression(fl);

		//Get out if bad
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid constant expression in enumerator";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}
		
		return 1;
	} else {
		//Otherwise, push back and leave
		push_back_token(fl, lookahead);
		return 1;
	}
}



/**
 * Helper to maintain RL(1) properties. Remember, by the time we've gotten here, we've already seen a COMMA
 *
 * BNF Rule: <enumerator-list-prime> ::= ,<enumerator><enumerator-list-prime>
 */
u_int8_t enumeration_list_prime(FILE* fl){
	Lexer_item l;
	parse_message_t message;
	u_int8_t status = 0;

	//We now need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid enumerator in enumeration list";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Now if we see a comma, we know that we have an enumerator-list-prime
	l = get_next_token(fl, &parser_line_num);

	//If we see a comma, we'll use the helper
	if(l.tok == COMMA){
		return enumeration_list_prime(fl);
	} else {
		//Put it back and get out if no
		push_back_token(fl, l);
		return 1;
	}
}


/**
 * An enumeration list guarantees that we have at least one enumerator
 *
 * BNF Rule: <enumerator-list> ::= <enumerator><enumerator-list-prime>
 */
u_int8_t enumeration_list(FILE* fl){
	Lexer_item l;
	parse_message_t message;
	u_int8_t status = 0;

	//We need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid enumerator in enumeration list";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Now if we see a comma, we know that we have an enumerator-list-prime
	l = get_next_token(fl, &parser_line_num);

	//If we see a comma, we'll use the helper
	if(l.tok == COMMA){
		return enumeration_list_prime(fl);
	} else {
		//Put it back and get out if no
		push_back_token(fl, l);
		return 1;
	}
}


/**
 * An enumeration specifier will always start with enumerated
 * REMEMBER: Due to RL(1), by the time we get here ENUMERATED has already been seen
 *
 * BNF Rule: <enumator-specifier> ::= enumerated <identifier> { <enumerator-list> } 
 * 						  			| enumerated <identifier>
 *
 * TODO SYMTAB
 * 						  			
 */
u_int8_t enumeration_specifier(FILE* fl){
	Lexer_item l;
	parse_message_t message;


	//We now have to see a valid identifier, since we've already seen the ENUMERATED keyword
	u_int8_t status = identifier(fl);

	//If it's bad then we're done here
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid identifier in enumeration specifier";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//Following this, if we see a right curly brace, we know that we have a list
	l = get_next_token(fl, &parser_line_num);

	if(l.tok == L_CURLY){
		//Push onto the grouping stack for matching
		push(grouping_stack, l);

		//We now must see a valid enumeration list
		status = enumeration_list(fl);

		//If it's bad then we're done here
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid enumeration list in enumeration specifier";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		l = get_next_token(fl, &parser_line_num);

		//All of our fail cases here
		if(l.tok != R_CURLY){
			message.message = PARSE_ERROR;
			message.info = "Right curly brace expected at end of enumeration list";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Unmatched left curly
		if(pop(grouping_stack).tok != L_CURLY){
			message.message = PARSE_ERROR;
			message.info = "Unmatched right parenthesis";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Otherwise we should be fine when we get here, so we can return
		return 0;
		
	} else {
		//Otherwise, push it back and let someone else handle it
		push_back_token(fl, l);
		return 1;
	}
}


/**
 * Type specifiers can be the set of primitives or user defined types
 * TODO SYMTAB stuff
 *
 * BNF Rule:  <type-specifier> ::= void
 * 								 | u_int8
 * 								 | s_int8
 * 								 | u_int16
 * 								 | s_int16
 * 								 | u_int32
 * 								 | s_int32
 * 								 | u_int64
 * 								 | s_int64
 * 								 | float32
 * 								 | float64
 * 								 | char
 * 								 | str
 * 								 | <enumeration-specifier>
 * 								 | <structure-specifier>
 * 								 | <user-defined-type>
 */
u_int8_t type_specifier(FILE* fl){
	parse_message_t message;
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	u_int8_t status = 0;

	//In the case that we have one of the primitive types
	if(l.tok == VOID || l.tok == U_INT8 || l.tok == S_INT8 || l.tok == U_INT16 || l.tok == S_INT16
	  || l.tok == U_INT32 || l.tok == S_INT32 || l.tok == U_INT64 || l.tok == S_INT64 || l.tok == FLOAT32
	  || l.tok == FLOAT64 || l.tok == CHAR || l.tok == STR){
		//TODO put in symtable
		return 1;
	}

	//Otherwise, we still have some options here
	//If we see enumerated, we know it's an enumerated type
	if(l.tok == ENUMERATED){
		status = enumeration_specifier(fl);

		//If it's bad then we're done here
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid enumeration specifier in type specifier";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}	

		//Otherwise it worked so return 1
		return 1;
	}


	return 0;
}


/**
 * We can see several different storage class specifiers
 * 
 * BNF Rule: <storage-class-specifier> ::= static 
 * 								 		| external
 * 								 		| register
 */
u_int8_t storage_class_specifier(FILE* fl){
	Lexer_item l = get_next_token(fl, &parser_line_num);

	//If we see one here
	if(l.tok == STATIC || l.tok == EXTERNAL | l.tok == REGISTER){
		//TODO store in symtab
		return 1;
	} else {
		//Otherwise, put the token back and get out
		push_back_token(fl, l);
		return 0;
	}
}


/**
 * For a parameter declaration, we can see this items in order
 *
 * BNF Rule: <parameter-declaration> ::= (<storage-class-specifier>)? (constant)? <type-specifier> <direct-declarator>
 */
u_int8_t parameter_declaration(FILE* fl){
	Lexer_item lookahead;
	parse_message_t message;
	u_int8_t is_const = 0;
	u_int8_t status = 0;

	//We can optionally see a storage_class_specifier here
	storage_class_specifier(fl);

	//Now we can optionally see const here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We'll flag this for now TODO must go in symtab
	if(lookahead.tok == CONSTANT){
		is_const = 1;
	} else {
		//Put it back and move on
		push_back_token(fl, lookahead);
	}
	
	//Now we must see a valid type specifier
	status = type_specifier(fl);
	
	//If it's bad then we're done here
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid type specifier found in parameter declaration";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Finally, we must see a direct declarator that is valid
	status = direct_declarator(fl);

	//If it's bad then we're done here
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid direct declarator found in parameter declaration";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	return 1;
}


/**
 * Optional repetition allowed with our parameter list
 *
 * BNF Rule: <parameter-list-prime> ::= , <parameter-declaration><parameter-list-prime>
 * 										| epsilon
 */
u_int8_t parameter_list_prime(FILE* fl){
	Lexer_item lookahead;
	u_int8_t status;
	parse_message_t message;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If we see a comma, we know that this is the recursive step
	if(lookahead.tok == COMMA){
		//We need to now see a valid parameter declaration
		status = parameter_declaration(fl);
		
		//If it went wrong
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid parameter declaration in parameter list";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}
		
		//Now we will take our recursive step in seeing a parameter list prime
		return parameter_list_prime(fl);
	}

	//This means that we had an epsilon here, so we'll put the token back and leave
	push_back_token(fl, lookahead);
	return 1;
}


/**
 * BNF Rule: <parameter-list> ::= <parameter-declaration>(<parameter-list-prime>)?
 */
u_int8_t parameter_list(FILE* fl){
	u_int8_t status;
	parse_message_t message;

	//First, we must see a valid parameter declaration
	status = parameter_declaration(fl);
	
	//If we didn't see a valid one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid parameter declaration in parameter list";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Now we can see our parameter list
	return parameter_list_prime(fl);
}


/**
 * A function specifier can be either a STATIC or an EXTERNAL definition
 *
 * BNF rule: <function-specifier> ::= static 
 *			        			  | external
 */
u_int8_t function_specifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	parse_message_t message;
	
	//If we find one of these, push it to the stack and return 1
	if(l.tok == STATIC || l.tok == EXTERNAL){
		//Push to the stack and let the caller decide how to handle
		push(variable_stack, l);
		return 1;
	}
	
	//Otherwise we have something bad here
	message.message = PARSE_ERROR;
	message.info = "Invalid function specifier";
	num_errors++;

	return 0;
}


/**
 * Handle the case where we declare a function
 *
 * BNF Rule: <function-definition> ::= func (<function-specifier>)? <identifier> (<parameter-list>?) -> <type-specifier> <compound-statement>
 */
u_int8_t function_declaration(FILE* fl){
	Lexer_item lookahead;
	Lexer_item lookahead2;
	char* function_name;
	Lexer_item ident;
	parse_message_t message;
	//We may need this in later iterations
	Token function_storage_type = BLANK;
	u_int8_t status;
	//This will be used for error printing
	char info[500];
	
	//REMEMBER: by the time we get here, we've already seen and consumed "FUNC"
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We've seen the option function specifier
	if(lookahead.tok == COLON){
		status = function_specifier(fl);

		//If this happened then we have an issue
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid function specifier";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Otherwise we can grab out what we have here
		function_storage_type = pop(variable_stack).tok;

	} else {
		//Otherwise put the token back in the stream
		push_back_token(fl, lookahead);
	}
	
	//Now we must see an identifer
	status = identifier(fl);
	
	//We have no identifier, so we must quit
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "No valid identifier found";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//At this point we'll need to store in symtable
	//Pop this off the stack
	ident = pop(variable_stack);
	//Let's see if this already exists
	symtab_record_t* record = lookup(symtab, ident.lexeme);

	//This means that we were trying to redefine a function
	if(record != NULL){
		message.message = PARSE_ERROR;
		memset(info, 0, 500*sizeof(char));
		sprintf(info, "Function %s has already been defined on line %d", record->name, record->line_number);
		message.info = info;
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//Save this as function name
	function_name = ident.lexeme;

	//Now we need to see a valid parentheis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't find it, no point in going further
	if(lookahead.tok != L_PAREN){
		message.message = PARSE_ERROR;
		message.info = "Left parenthesis expected";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//SPECIAL CASE -- we could have a blank parameter list, in which case we're done
	lookahead2 = get_next_token(fl, &parser_line_num);
	
	if(lookahead2.tok == R_PAREN){
		//TODO insert blank parameter list
		goto arrow_ident;
	} else {
		push_back_token(fl, lookahead2);
	}
	
	//Otherwise we'll push this onto the grouping stack to check later
	push(grouping_stack, lookahead);
	
	//We'll deal with our storage of the function's name later, but at least now we know it's unique
	//We must now see a valid parameter list
	status = parameter_list(fl);

	//We have a bad parameter list
	if(status == 0){
		message.message = PARSE_ERROR;
		memset(info, 0, 500*sizeof(char));
		sprintf(info, "No valid paramter list found for function \"%s\"", function_name);
		message.info = info;
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//Now we need to see a valid closing parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't have an R_Paren that's an issue
	if(lookahead.tok != R_PAREN){
		message.message = PARSE_ERROR;
		message.info = "Right parenthesis expected";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//If this happens, then we have some unmatched parenthesis
	if(pop(grouping_stack).tok != L_PAREN){
		message.message = PARSE_ERROR;
		message.info = "Unmatched opening parenthesis found";
		message.line_num = parser_line_num;
		print_parse_message(&message);

		num_errors++;
		return 0;
	}


	//Past the point where we've seen the param_list
arrow_ident:
	//Grab the next one
	lookahead = get_next_token(fl, &parser_line_num);

	//We absolutely must see an arrow here
	if(lookahead.tok != ARROW){
		message.message = PARSE_ERROR;
		message.info = "Arrow expected after function declaration";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}



	return 0;
	
}


/**
 * Here we can either have a function definition or a declaration
 *
 * <declaration-partition>::= <function-definition>
 *                        	| <declaration>
 */
u_int8_t declaration_partition(FILE* fl){
	Lexer_item lookahead;
	u_int8_t status;
	parse_message_t message;

	lookahead = get_next_token(fl, &parser_line_num);

	//We know that we have a function here
	if(lookahead.tok == FUNC){
		status = function_declaration(fl);
	} else {
		//Push it back
		push_back_token(fl, lookahead);
		//Otherwise, the only other option is a declaration
		status = declaration(fl);
	}
	
	//Something failed
	if(status == 0){
		//Otherwise we've failed completely
		message.message = PARSE_ERROR;
		message.info = "Declaration Partition could not find a valid function or declaration";
		message.line_num = parser_line_num;
		print_parse_message(&message);

		num_errors++;
		return 0;
	}
	
	//If we get here it worked
	return 1;
}


/**
 * Here is our entry point
 *
 * BNF Rule: <program>::= {<declaration-partition>}*
 */
u_int8_t program(FILE* fl){
	Lexer_item l;
	u_int8_t status = 0;
	parse_message_t message;

	//As long as we aren't done
	while((l = get_next_token(fl, &parser_line_num)).tok != DONE){
		//Put the token back
		push_back_token(fl, l);

		//Pass along and let the rest handle
		status = declaration_partition(fl);
		
		//If we have an error then we'll print it out
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Program rule encountered an error from declaration partition";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			//If we have but one failure, the whole thing is toast
			break;
		}
	}

	return status;
}


/**
 * Entry point for our parser. Everything beyond this point will be called in a recursive-descent fashion through
 * static methods
*/
u_int8_t parse(FILE* fl){
	u_int8_t status = 0;
	num_errors = 0;
	parse_message_t message;

	//Initialize our global symtab here
	symtab = initialize_symtab();
	//Also create a stack for our matching uses(curlies, parens, etc.)
	variable_stack = create_stack();
	grouping_stack = create_stack();

	//Global entry/run point
	status = program(fl);

	//If we failed
	if(status == 0){
		char info[500];
		sprintf(info, "Parsing failed with %d errors", num_errors);
		printf("\n\n=======================================================================\n");
		printf("%s\n", info);
		printf("=======================================================================\n\n");
	}
	
	//Clean these both up for memory safety
	destroy_stack(variable_stack);
	destroy_stack(grouping_stack);
	destroy_symtab(symtab);
	
	return status;
}
