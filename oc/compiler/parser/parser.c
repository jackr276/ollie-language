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
static u_int8_t declaration(FILE* fl);
static u_int8_t compound_statement(FILE* fl);
static u_int8_t statement(FILE* fl);
static u_int8_t initializer(FILE* fl);
static u_int8_t declarator(FILE* fl);
static u_int8_t direct_declarator(FILE* fl);

/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_type_t message_type, char* info){
	//Build and populate the message
	parse_message_t parse_message;
	parse_message.message = message_type;
	parse_message.info = info;
	parse_message.line_num = parser_line_num;

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
 * Do we have an identifier or not?
 */
static u_int8_t identifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	char info[500];
	
	//If we can't find it that's bad
	if(l.tok != IDENT){
		sprintf(info, "String %s is not a valid identifier", l.lexeme);
		print_parse_message(PARSE_ERROR, info);
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//Grab the star
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok == STAR){
		//TODO handle it
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
	Lexer_item l;
	
	//We should see one of the 4 constants here
	l = get_next_token(fl, &parser_line_num);

	//Do this for now, later on we'll need symtable integration
	if(l.tok == INT_CONST || l.tok == STR_CONST || l.tok == CHAR_CONST || l.tok == FLOAT_CONST ){
		return 1;
	}

	//Otherwise here, we have an error
	print_parse_message(PARSE_ERROR, "Invalid constant found");
	num_errors++;
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
	Lexer_item lookahead;
	u_int8_t status;

	//We must first see a valid assignment-expression
	status = assignment_expression(fl);
	
	//It failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid assignment expression found in expression");
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
	Lexer_item lookahead;
	u_int8_t status;

	//We must first see a valid assignment-expression
	status = assignment_expression(fl);
	
	//It failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid assignment expression found in expression");
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
			print_parse_message(PARSE_ERROR, "Invalid identifier found in primary expression");
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
			print_parse_message(PARSE_ERROR, "Invalid constant found in primary expression");
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
			print_parse_message(PARSE_ERROR, "Invalid expression found in primary expression");
			num_errors++;
			return 0;
		}

		//We must now also see an R_PAREN, and ensure that we have matching on the stack
		lookahead = get_next_token(fl, &parser_line_num);

		//If it isn't a right parenthesis
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression");
			num_errors++;
			return 0;
		//Make sure we match
		} else if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression");
			num_errors++;
			return 0;
		}
		
		//Otherwise we're all set
		return 1;
	} else {
		char info[500];
		memset(info, 0, 500 * sizeof(char));
		sprintf(info, "Invalid token with lexeme %s found in primary expression", lookahead.lexeme); 
		print_parse_message(PARSE_ERROR, info);
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
			print_parse_message(PARSE_ERROR, "Invalid unary expression found in assignment expression");
			num_errors++;
			return 0;
		}

		//Now we must see the := assignment operator
		lookahead = get_next_token(fl, &parser_line_num);

		//We have a bad one
		if(lookahead.tok != COLONEQ){
			print_parse_message(PARSE_ERROR, "Assignment operator := expected after unary expression");
			num_errors++;
			return 0;
		}

		//Otherwise it worked just fine
		//Now we must see an assignment expression again
		status = assignment_expression(fl);

		//We have a bad one
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid conditional expression found in assingment expression");
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
			print_parse_message(PARSE_ERROR, "Invalid conditional expression found in postfix expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid primary expression no matter what
	status = primary_expression(fl);

	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid primary expression found in postifx expression");
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
				print_parse_message(PARSE_ERROR, "Right parenthesis expected after primary expression");
				num_errors++;
				return 0;
			//Some unmatched parenthesis here
			} else if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected");
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
					print_parse_message(PARSE_ERROR, "Invalid expression in primary expression index");
					num_errors++;
					return 0;
				}

				//Now we have to see a valid right bracket
				lookahead = get_next_token(fl, &parser_line_num);

				//Just to double check
				if(lookahead.tok != R_BRACKET){
					print_parse_message(PARSE_ERROR, "Right bracket expected after primary expression index");
					num_errors++;
					return 0;
				//Or we have some unmatched grouping operator
				} else if(pop(grouping_stack).tok != L_BRACKET){
					print_parse_message(PARSE_ERROR, "Unmatched bracket detected");
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
				print_parse_message(PARSE_ERROR, "Invalid unary expression following preincrement/predecrement");
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
				print_parse_message(PARSE_ERROR, "Left parenthesis expected after size keyword");
				num_errors++;
				return 0;
			}
			
			//Push it onto the stack
			push(grouping_stack, lookahead);
			
			//Now we must see a valid unary expression
			status = unary_expression(fl);

			//If it was bad
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid unary expression given to size operator");
				num_errors++;
				return 0;
			}
		
			//If we get here though we know it worked
			//Now we must see a valid closing parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			//If this is an R_PAREN
			if(lookahead.tok != R_PAREN) {
				print_parse_message(PARSE_ERROR, "Right parenthesis expected after unary expression");
				num_errors++;
				return 0;
			//Otherwise if it wasn't matched right
			} else if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected");
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
				print_parse_message(PARSE_ERROR, "Left parenthesis expected after typesize keyword");
				num_errors++;
				return 0;
			}
			
			//Push it onto the stack
			push(grouping_stack, lookahead);
			
			//Now we must see a valid type name 
			status = type_name(fl);

			//If it was bad
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid type name given to typesize operator");
				num_errors++;
				return 0;
			}
		
			//If we get here though we know it worked
			//Now we must see a valid closing parenthesis
			lookahead = get_next_token(fl, &parser_line_num);

			//If this is an R_PAREN
			if(lookahead.tok != R_PAREN) {
				print_parse_message(PARSE_ERROR, "Right parenthesis expected after type name");
				num_errors++;
				return 0;
			//Otherwise if it wasn't matched right
			} else if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected");
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
				print_parse_message(PARSE_ERROR, "Invalid cast expression following unary operator");
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
				print_parse_message(PARSE_ERROR, "Invalid postfix expression inside of unary expression");
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
			print_parse_message(PARSE_ERROR, "Invalid type name found in cast expression");
			num_errors++;
			return 0;
		}
		
		//Otherwise we're good to keep going
		//We now must see a valid R_PAREN
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If this is an R_PAREN
		if(lookahead.tok != R_PAREN) {
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after type name");
			num_errors++;
			return 0;
		} else if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis deteced");
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
		print_parse_message(PARSE_ERROR, "Invalid unary expression found in cast expression"); 
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid cast expression
	status = cast_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid cast expression found in multiplicative expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid cast expression
	status = cast_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid cast expression found in multiplicative expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//First we must see a valid multiplicative expression
	status = multiplicative_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid mutliplicative expression found in additive expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//First we must see a valid multiplicative expression
	status = multiplicative_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid multiplicative expression found in additive expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid additive expression
	status = additive_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid additive expression found in shift expression");
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
		print_parse_message(PARSE_ERROR, "Invalid additive expression found in shift expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid shift expression
	status = shift_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid shift expression found in relational expression");
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
		print_parse_message(PARSE_ERROR, "Invalid shift expression found in relational expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid relational-expression
	status = relational_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid relational expression found in equality expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid relational-expression
	status = relational_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid relational expression found in equality expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid equality-expression
	status = equality_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid equality expression found in and expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid equality-expression
	status = equality_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid equality expression found in and expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid and-expression
	status = and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid and expression found in exclusive or expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid and-expression
	status = and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid and expression found in exclusive or expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid exclusive or expression
	status = exclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid exclusive or expression found in inclusive or expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = exclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid exclusive or expression found in inclusive or expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = inclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid inclusive or expression found in logical and expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid inclusive or expression
	status = inclusive_or_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid inclusive or expression found in logical expression");
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
	Lexer_item lookahead;
	u_int8_t status;

	//We now must see a valid logical and expression
	status = logical_and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid logical and expression found in logical or expression");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We first must see a valid logical and expression
	status = logical_and_expression(fl);
	
	//We have a bad one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid logical and expression found in logical or expression");
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
	//Pass through to the logical or expression expression
	u_int8_t status = logical_or_expression(fl);

	//Something failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid logical or expression found in conditional expression");
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
	//Pass through to the conditional expression
	u_int8_t status = conditional_expression(fl);

	//Something failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression found in constant expression");
		num_errors++;
		return 0;
	}

	//Otherwise we're all set
	return 1;
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


/**
 * For an enumerator, we can see an ident or an assigned ident
 *
 * BNF Rule: <enumerator> ::= <identifier> 
 * 			           	  | <identifier> := <constant-expression>
 */
u_int8_t enumerator(FILE* fl){
	Lexer_item lookahead;
	u_int8_t status;

	//We must see a valid identifier here
	status = identifier(fl);

	//Get out if bad
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid identifier in enumerator");
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
			print_parse_message(PARSE_ERROR, "Invalid constant expression in enumerator");
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
	u_int8_t status = 0;

	//We now need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid enumerator in enumeration list");
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
	u_int8_t status = 0;

	//We need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid enumerator in enumeration list");
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

	//We now have to see a valid identifier, since we've already seen the ENUMERATED keyword
	u_int8_t status = identifier(fl);

	//If it's bad then we're done here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid identifier in enumeration specifier");
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
			print_parse_message(PARSE_ERROR, "Invalid enumeration list in enumeration specifier");
			num_errors++;
			return 0;
		}

		l = get_next_token(fl, &parser_line_num);

		//All of our fail cases here
		if(l.tok != R_CURLY){
			print_parse_message(PARSE_ERROR, "Right curly brace expected at end of enumeration list");
			num_errors++;
			return 0;
		}

		//Unmatched left curly
		if(pop(grouping_stack).tok != L_CURLY){
			print_parse_message(PARSE_ERROR, "Unmatched right parenthesis");
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
			print_parse_message(PARSE_ERROR, "Invalid enumeration specifier in type specifier");
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
		print_parse_message(PARSE_ERROR, "Invalid type specifier found in parameter declaration");
		num_errors++;
		return 0;
	}

	//Finally, we must see a direct declarator that is valid
	status = direct_declarator(fl);

	//If it's bad then we're done here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid direct declarator found in parameter declaration");
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

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If we see a comma, we know that this is the recursive step
	if(lookahead.tok == COMMA){
		//We need to now see a valid parameter declaration
		status = parameter_declaration(fl);
		
		//If it went wrong
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid parameter declaration in parameter list");
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
	u_int8_t status = 0;

	//First, we must see a valid parameter declaration
	status = parameter_declaration(fl);
	
	//If we didn't see a valid one
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid parameter declaration in parameter list");
		num_errors++;
		return 0;
	}

	//Now we can see our parameter list
	return parameter_list_prime(fl);
}


/**
 * BNF Rule: <expression-statement> ::= {<expression>}?;
 */
static u_int8_t expression_statement(FILE* fl){
	return 1;

}


/**
 * <labeled-statement> ::= <label-identifier> : <statement> 
 * 						 | case <constant-expression> : <statement> 
 * 						 | default : <statement>
 */
static u_int8_t labeled_statement(FILE* fl){
	return 0;

}


/**
 * BNF Rule: <if-statement> ::= if( <expression> ) then <statement> {else <statement>}*
 */
static u_int8_t if_statement(FILE* fl){
	return 0;

}


/**
 * BNF Rule: <jump-statement> ::= jump <identifier> 
 * 								| continue when(<expression>); 
 * 								| continue; 
 * 								| break when(<expression>); 
 * 								| break; 
 * 								| ret {<expression>}?;
 */
static u_int8_t jump_statement(FILE* fl){
	return 0;

}

/**
 * BNF Rule: <switch-statement> ::= switch on( <expression> ) <labeled-statement>
 */
static u_int8_t switch_statement(FILE* fl){
	return 0;

}


/**
 * BNF Rule: <iterative-statement> ::= while( <expression> ) do <statement> 
 * 									 | do <statement> while( <expression> ) 
 * 									 | for( {<expression>}? ; {<expression>}? ; {<expression>}? ) do <statement>
 */
static u_int8_t iterative_statement(FILE* fl){
	return 0;
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
			print_parse_message(PARSE_ERROR, "Invalid compound statement found in statement");
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
			print_parse_message(PARSE_ERROR, "Invalid labeled statement found in statement");
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
			print_parse_message(PARSE_ERROR, "Invalid if statement found in statement");
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
			print_parse_message(PARSE_ERROR, "Invalid switch statement found in statement");
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
			print_parse_message(PARSE_ERROR, "Invalid jump statement found in statement");
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
		status = jump_statement(fl);

		//If it fails
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid jump statement found in statement");
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
			print_parse_message(PARSE_ERROR, "Invalid jump statement found in statement");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//When we get here, we absolutely must see a cury brace
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Opening curly brace expected to begin compound statement");
		num_errors++;
		return 0;
	}

	//We'll push this guy onto the stack for later
	push(grouping_stack, lookahead);
	//TODO change the lexical scope here
	
	//Grab the next token to search
	lookahead = get_next_token(fl, &parser_line_num);

	//Now we keep going until we see the closing curly brace
	while(lookahead.tok != R_CURLY){
		//If we see this we know that we have a declaration
		if(lookahead.tok == LET || lookahead.tok == DECLARE){
			//Push it back
			push_back_token(fl, lookahead);

			//Hand it off to the declaration function
			status = declaration(fl);
			
			//If we fail here just leave
			if(status == 0){
				print_parse_message(PARSE_ERROR, "Invalid declaration found in compound statement");
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
				print_parse_message(PARSE_ERROR, "Invalid statement found in compound statement");
				num_errors++;
				return 0;
			}
			//Otherwise we're all good
		}

		//Grab the next token to refresh the search
		lookahead = get_next_token(fl, &parser_line_num);
	}
	
	//When we make it here, we know that we have an R_CURLY in the lookahead
	//Let's check to see if the grouping went properly
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected inside of compound statement");
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
 * 								  | <identifier> ( {<identifier>}*{, <identifier>}* )
 */
u_int8_t direct_declarator(FILE* fl){
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
			print_parse_message(PARSE_ERROR, "Invalid declarator found inside of direct declarator");
			num_errors++;
			return 0;
		}

		//Now we must see a valid closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//No closing paren
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Closing parenthesis expected after declarator");
			num_errors++;
			return 0;
		}

		//Unmatched grouping operator
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected");
			num_errors++;
			return 0;
		}
		
		//Otherwise if we get here we're set
		return 1;

	//The other option, we have an ident
	} else if(lookahead.tok == IDENT){
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
						print_parse_message(PARSE_ERROR, "Invalid constant expression in array subscript");
						return 0;
					}

					//Otherwise, we now have to see an R_BRACKET
					lookahead = get_next_token(fl, &parser_line_num);

					//If we don't see a ]
					if(lookahead.tok != R_BRACKET){
						print_parse_message(PARSE_ERROR, "Right bracket expected to close array subscript");
						num_errors++;
						return 0;
					}

					//If they don't match
					if(pop(grouping_stack).tok != L_BRACKET){
						print_parse_message(PARSE_ERROR, "Unmatched brackets detected");
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
						print_parse_message(PARSE_ERROR, "Identifier expected after comma in identifier list");
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
				//If not, we have to see this
				status = parameter_list(fl);
				
				//If it failed
				if(status == 0){
					print_parse_message(PARSE_ERROR, "Invalid parameter list in function declarative");
					num_errors++;
					return 0;
				}
			}

			//Whatever happened, we need to see a closing paren here
			lookahead = get_next_token(fl, &parser_line_num);

			//If we don't see a )
			if(lookahead.tok != R_PAREN){
				print_parse_message(PARSE_ERROR, "Right parenthesis expected");
				num_errors++;
				return 0;
			}

			//If they don't match
			if(pop(grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected");
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
		print_parse_message(PARSE_ERROR, "Identifier or declarator expected in direct declarator");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid initializer
	status = initializer(fl);

	//Invalid here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid initializer in initializer list");
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
	Lexer_item lookahead;
	u_int8_t status = 0;

	//We must first see a valid initializer
	status = initializer(fl);

	//Invalid here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid initializer in initializer list");
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
 * An initializer can descend into an assignment expression or an initializer list
 *
 * BNF Rule: <initializer> ::= <conditional-expression> 
 * 							| { <intializer-list> }
 */
static u_int8_t initializer(FILE* fl){
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
			print_parse_message(PARSE_ERROR, "Invalid initializer list in initializer");
			num_errors++;
			return 0;
		}
		
		//Now we have to see a closing curly
		lookahead = get_next_token(fl, &parser_line_num);

		//If we don't see it
		if(lookahead.tok != R_CURLY){
			print_parse_message(PARSE_ERROR, "Closing curly brace expected after initializer list");
			num_errors++;
			return 0;
		}
		
		//Unmatched curlies here
		if(pop(grouping_stack).tok != L_CURLY){
			print_parse_message(PARSE_ERROR, "Unmatched curly braces detected");
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
			print_parse_message(PARSE_ERROR, "Invalid conditional expression found in initializer");
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
	u_int8_t status = 0;

	//We can see pointers here
	status = pointer(fl);

	//If we see any pointers, handle them accordingly TODO
	
	//Now we must see a valid direct declarator
	status = direct_declarator(fl);
	
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid direct declarator found in declarator");
		num_errors++;
		return 0;
	}

	//Otherwise we're all set so return 1
	return 1;
}


/**
 * A declaration is the other main kind of block that we can see other than functions
 *
 * BNF Rule: <declaration> ::= declare {constant}? <storage-class-specifier>? <type-specifier> <declarator>; 
 * 							 | let {constant}? <storage-class-specifier>? <type-specifier> <declarator> := <intializer>;
 */
static u_int8_t declaration(FILE* fl){
	Lexer_item l;
	u_int8_t status = 0;
	Token let_or_declare;

	//Grab the token
	l = get_next_token(fl, &parser_line_num);

	//Something bad here
	if(l.tok != LET && l.tok != DECLARE){
		print_parse_message(PARSE_ERROR, "Declare or let keywords expected in declaration");
		num_errors++;
		return 0;
	}

	//Save this
	let_or_declare = l.tok;

	//Grab the next token
	l = get_next_token(fl, &parser_line_num);

	//We can see constant here optionally
	if(l.tok == CONSTANT){
		//Handle accordingly
		//Grab the next token
		l = get_next_token(fl, &parser_line_num);
	} else {
		//Push it back if it isn't the constant keyword
		push_back_token(fl, l);
	}

	//We know we're clear if we get here
	
	//We can now see a storage class specifier
	status = storage_class_specifier(fl);
	//Handle accordingly
	
	//We now must see a valid type specifier
	status = type_specifier(fl);
	
	//If bad
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid type specifier in declaration");
		num_errors++;
		return 0;
	}

	//Now we must see a valid declarator
	status = declarator(fl);

	//If bad
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid declarator in declaration");
		num_errors++;
		return 0;
	}
		
	//Now we can take two divergent paths here
	if(let_or_declare == LET){
		//Now we must see the assignment operator
		l = get_next_token(fl, &parser_line_num);

		//If we don't see it, get out
		if(l.tok != COLONEQ){
			print_parse_message(PARSE_ERROR, "Assignment operator(:=) expected after declaration");
			num_errors++;
			return 0;
		}
		
		//Now we must see a valid initializer
		status = initializer(fl);

		//If we don't see it, get out
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid initializer in declaration");
			num_errors++;
			return 0;
		}

		//We now must see a semicolon
		l = get_next_token(fl, &parser_line_num);

		if(l.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected at the end of a declaration");
			num_errors++;
			return 0;
		}
	
		//Otherwise it worked and we can leave
		return 1;

	}

	//If we had a declare statement
	if(let_or_declare == DECLARE){
		//If it was a declare statement, we must only see the semicolon to exit
		l = get_next_token(fl, &parser_line_num);

		if(l.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected at the end of declaration");
			num_errors++;
			return 0;
		}
	
		//Otherwise it worked and we can leave
		return 1;
	}

	//For compiler only, should never get here
	return 0;
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
	
	//If we find one of these, push it to the stack and return 1
	if(l.tok == STATIC || l.tok == EXTERNAL){
		//Push to the stack and let the caller decide how to handle
		push(variable_stack, l);
		return 1;
	}

	//This isn't a necessity so no error
	return 0;
}


/**
 * Handle the case where we declare a function
 *
 * BNF Rule: <function-definition> ::= func (<function-specifier>)? <identifier> (<parameter-list>?) -> <type-specifier> <compound-statement>
 *
 * REMEMBER: By the time we get here, we've already seen the func keyword
 *
 * We will also handle function specifiers internally, an additional method would be silly
 *
 * BNF rule: <function-specifier> ::= static 
 *			        			  | external
 */
u_int8_t function_declaration(FILE* fl){
	Lexer_item lookahead;
	Lexer_item lookahead2;
	char* function_name;
	Lexer_item ident;
	//We may need this in later iterations
	Token function_storage_type = BLANK;
	u_int8_t status;
	//This will be used for error printing
	char info[500];
	
	//REMEMBER: by the time we get here, we've already seen and consumed "FUNC"
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We've seen the option function specifier
	if(lookahead.tok == COLON){
		//Let's find a function specifier
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If we find one of these, push it to the stack and return 1
		if(lookahead.tok == STATIC || lookahead.tok == EXTERNAL){
			//TODO handle with symtable
		} else {
			print_parse_message(PARSE_ERROR, "Function specifier STATIC or EXTERNAL expected after colon");
			num_errors++;
			return 0;
		}
	//Otherwise it's a plain function
	} else {
		//Otherwise put the token back in the stream
		push_back_token(fl, lookahead);
	}
	
	//Now we must see an identifer
	status = identifier(fl);
	
	//We have no identifier, so we must quit
	if(status == 0){
		print_parse_message(PARSE_ERROR, "No valid identifier found");
		num_errors++;
		return 0;
	}

	//TODO symtable stuff

	//Now we need to see a valid parentheis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't find it, no point in going further
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected");
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
		print_parse_message(PARSE_ERROR, "No valid parameter list found for function");
		num_errors++;
		return 0;
	}
	
	//Now we need to see a valid closing parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't have an R_Paren that's an issue
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected");
		num_errors++;
		return 0;
	}
	
	//If this happens, then we have some unmatched parenthesis
	if(pop(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis found");
		num_errors++;
		return 0;
	}


	//Past the point where we've seen the param_list
arrow_ident:
	//Grab the next one
	lookahead = get_next_token(fl, &parser_line_num);

	//We absolutely must see an arrow here
	if(lookahead.tok != ARROW){
		print_parse_message(PARSE_ERROR, "Arrow expected after function declaration");
		num_errors++;
		return 0;
	}

	//After the arrow we must see a valid type specifier
	status = type_specifier(fl);

	//If it failed
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid return type given to function");
		num_errors++;
		return 0;
	}

	//Now we must see a compound statement
	status = compound_statement(fl);

	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid compound statement in function");
		num_errors++;
		return 0;
	}

	//All went well if we make it here
	return 1;
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
		print_parse_message(PARSE_ERROR, "Declaration Partition could not find a valid function or declaration");
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

	//As long as we aren't done
	while((l = get_next_token(fl, &parser_line_num)).tok != DONE){
		//Put the token back
		push_back_token(fl, l);

		//Pass along and let the rest handle
		status = declaration_partition(fl);
		
		//If we have an error then we'll print it out
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid declaration partition found");
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
