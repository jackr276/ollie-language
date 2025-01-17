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

//The root of the entire tree
generic_ast_node_t* ast_root = NULL;


//Function prototypes are predeclared here as needed to avoid excessive restructuring of program
static generic_ast_node_t* cast_expression(FILE* fl);
static generic_ast_node_t* type_specifier(FILE* fl);
static generic_ast_node_t* assignment_expression(FILE* fl);
static generic_ast_node_t* conditional_expression(FILE* fl);
static generic_ast_node_t* unary_expression(FILE* fl);
static generic_ast_node_t* declaration(FILE* fl);
static generic_ast_node_t* compound_statement(FILE* fl);
static generic_ast_node_t* statement(FILE* fl);
static generic_ast_node_t* expression(FILE* fl);
static generic_ast_node_t* initializer(FILE* fl);
static generic_ast_node_t* declarator(FILE* fl);


/**
 * Simply prints a parse message in a nice formatted way
*/
static void print_parse_message(parse_message_type_t message_type, char* info, u_int16_t line_num){
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
 * We will always return a pointer to the node holding the identifier. Due to the times when
 * this will be called, we can not do any symbol table validation here. We will do a quick query and 
 * see if it is some defined variable
 *
 * BNF "Rule": <variable-identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* variable_identifier(FILE* fl){
	//In case of error printing
	char info[2000];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num);
	
	//If we can't find it that's bad
	if(lookahead.tok != IDENT){
		sprintf(info, "String %s is not a valid identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node that will be sent up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create the identifier node
	generic_ast_node_t* ident_node = ast_node_alloc(AST_NODE_CLASS_VARIABLE_IDENTIFIER); //Add the identifier into the node itself
	//Copy the string we got into it
	strcpy(((variable_identifier_ast_node_t*)(ident_node->node))->identifier, lookahead.lexeme);

	//Now we can look this up in the symbol table. Although we cannot make any value judgements about this here, we
	//can at least say if it's been defined or not
	symtab_variable_record_t* found = lookup_variable(variable_symtab, lookahead.lexeme);

	//This will either be NULL or it will be the record. In either case, we'll simply populate the record in the 
	//node and give it back
	((variable_identifier_ast_node_t*)(ident_node->node))->variable_record = found;

	//Return our reference to the node
	return ident_node;
}


/**
 * A label identifier will always be a child of some other node. As such, it will be
 * added on as a child of that node once created. We will return a reference to the 
 * node that was made here
 *
 * Although we cannot make any label judgments about whether or not it was defined in the 
 * symbol table, we can at least look to see if it was
 * BNF "Rule": <label_identifier> ::= ${(<letter>) | <digit> | _ | $}*
 */
static generic_ast_node_t* label_identifier(FILE* fl){
	//In case of error printing
	char info[2000];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num);
	
	//If we can't find it that's bad
	if(lookahead.tok != LABEL_IDENT){
		sprintf(info, "String %s is not a valid label-specific identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node to be propagated up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create the label identifier node
	generic_ast_node_t* label_ident_node = ast_node_alloc(AST_NODE_CLASS_LABEL_IDENTIFIER);

	//Add the identifier into the node itself
	strcpy(((label_identifier_ast_node_t*)(label_ident_node->node))->identifier, lookahead.lexeme);

	//Now we will hunt to see if we could actually find the label in the symbol table
	symtab_variable_record_t* found = lookup_variable(variable_symtab, lookahead.lexeme); 

	//If we didn't find anything, found will just be null, so either way we'll assign it here
	((label_identifier_ast_node_t*)(label_ident_node->node))->label_record = found;

	//Return the reference to the node that we made
	return label_ident_node;
}


/**
 * We will always return a pointer to the node holding the identifier. Due to the times when
 * this will be called, we can not do any symbol table validation here. We will do a quick query and 
 * see if it is some defined variable
 *
 * BNF "Rule": <function-identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* function_identifier(FILE* fl){
	//In case of error printing
	char info[2000];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num);
	
	//If we can't find it that's bad
	if(lookahead.tok != IDENT){
		sprintf(info, "String %s is not a valid function identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node that will be sent up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create the identifier node
	generic_ast_node_t* function_ident_node = ast_node_alloc(AST_NODE_CLASS_FUNCTION_IDENTIFIER); //Add the identifier into the node itself
	//Copy the string we got into it
	strcpy(((function_identifier_ast_node_t*)(function_ident_node->node))->identifier, lookahead.lexeme);

	//Now we can look this up in the symbol table. Although we cannot make any value judgements about this here, we
	//can at least say if it's been defined or not
	symtab_function_record_t* found = lookup_function(function_symtab, lookahead.lexeme);

	//This will either be NULL or it will be the record. In either case, we'll simply populate the record in the 
	//node and give it back
	((function_identifier_ast_node_t*)(function_ident_node->node))->func_record = found;

	//Return our reference to the node
	return function_ident_node;
}


/**
 * We will always return a pointer to the node holding the identifier. Due to the times when
 * this will be called, we can not do any symbol table validation here. We will do a quick query and 
 * see if it is some defined variable
 *
 * BNF "Rule": <type-identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* type_identifier(FILE* fl){
	//In case of error printing
	char info[2000];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num);
	
	//If we can't find it that's bad
	if(lookahead.tok != IDENT){
		sprintf(info, "String %s is not a valid type identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node that will be sent up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create the identifier node
	generic_ast_node_t* type_ident_node = ast_node_alloc(AST_NODE_CLASS_TYPE_IDENTIFIER); //Add the identifier into the node itself
	//Copy the string we got into it
	strcpy(((type_identifier_ast_node_t*)(type_ident_node->node))->identifier, lookahead.lexeme);

	//Now we can look this up in the symbol table. Although we cannot make any value judgements about this here, we
	//can at least say if it's been defined or not
	symtab_type_record_t* found = lookup_type(type_symtab, lookahead.lexeme);

	//This will either be NULL or it will be the record. In either case, we'll simply populate the record in the 
	//node and give it back
	((type_identifier_ast_node_t*)(type_ident_node->node))->type_record = found;

	//Return our reference to the node
	return type_ident_node;
}


/**
 * Handle a constant. There are 4 main types of constant, all handled by this function. A constant
 * is always the child of some parent node. We will always return the reference to the node
 * created here
 *
 * BNF Rule: <constant> ::= <integer-constant> 
 * 						  | <string-constant> 
 * 						  | <float-constant> 
 * 						  | <char-constant>
 */
static generic_ast_node_t* constant(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;
	
	//We should see one of the 4 constants here
	lookahead = get_next_token(fl, &parser_line_num);

	//Create our constant node
	generic_ast_node_t* constant_node = ast_node_alloc(AST_NODE_CLASS_CONSTANT);

	//We'll go based on what kind of constant that we have
	switch(lookahead.tok){
		case INT_CONST:
			((constant_ast_node_t*)constant_node->node)->constant_type = INT_CONST;
			break;
		case FLOAT_CONST:
			((constant_ast_node_t*)constant_node->node)->constant_type = FLOAT_CONST;
			break;
		case CHAR_CONST:
			((constant_ast_node_t*)constant_node->node)->constant_type = CHAR_CONST;
			break;
		case STR_CONST:
			((constant_ast_node_t*)constant_node->node)->constant_type = STR_CONST;
			break;
		default:
			print_parse_message(PARSE_ERROR, "Invalid constant given", parser_line_num);
			num_errors++;
			//Create and return an error node that will be propagated up
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we made it here, then we know that we have a valid constant
	//We'll now copy the lexeme that we saw in here to the constant
	strcpy(((constant_ast_node_t*)constant_node->node)->constant, lookahead.lexeme);

	//All went well so give the constant node back
	return constant_node;
}


/**
 * An expression decays into an assignment expression. An expression
 * node is more of a "pass-through" rule, and itself does not make any children. It does
 * however return the reference of whatever it created
 *
 * BNF Rule: <expression> ::= <assignment-expression>
 */
static generic_ast_node_t* expression(FILE* fl){
	u_int16_t current_line = parser_line_num;
	//Call the appropriate rule
	generic_ast_node_t* expression_node = assignment_expression(fl);
	
	//If it did fail, a message is appropriate here
	if(expression_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Top level expression invalid", current_line);
		return expression_node;
	}

	//Otherwise, we're all set so just give the node back
	return expression_node;
}


/**
 * A function call looks for a very specific kind of identifer followed by
 * parenthesis and the appropriate number of parameters for the function, each of
 * the appropriate type
 * 
 * By the time we get here, we will have already consumed the "@" token
 *
 * BNF Rule: <function-call> ::= @<function-identifier>({conditional-expression}*)
 *
 */
static generic_ast_node_t* function_call(FILE* fl){

}


/**
 * A primary expression is, in a way, the termination of our expression chain. However, it can be used 
 * to chain back up to an expression in general using () as an enclosure. Just like all rules, a primary expression
 * itself has a parent and will produce children. The reference to the primary expression itself is always returned
 *
 * BNF Rule: <primary-expression> ::= <identifier>
 * 									| <constant> 
 * 									| (<expression>)
 * 									| <function-call>
 */
static generic_ast_node_t* primary_expression(FILE* fl){
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//For error printing
	char info[2000];
	//Lookahead token
	Lexer_item lookahead;
	
	//Grab the next token, we'll multiplex on this
	lookahead = get_next_token(fl, &parser_line_num);

	//We've seen an ident, so we'll put it back and let
	//that rule handle it. This identifier will always be 
	//a variable. It must also be a variable that has been initialized.
	//We will check that it was initialized here
	if(lookahead.tok == IDENT){
		//Put it back
		push_back_token(fl, lookahead);

		//We will let the identifier rule actually grab the ident. In this case
		//the identifier will be a variable of some sort, that we'll need to check
		//against the symbol table
		generic_ast_node_t* variable = variable_identifier(fl);

		//If there was a failure of some kind, we'll allow it to propogate up
		if(variable->CLASS == AST_NODE_CLASS_ERR_NODE){
			return variable;
		}

		//We now must see a variable that was intialized. If it was not
		//initialized, then we have an issue
		if(((variable_identifier_ast_node_t*)(variable)->node)->variable_record == NULL){
			sprintf(info, "Variable \"%s\" has not been declared", ((variable_identifier_ast_node_t*)(variable)->node)->identifier);
			print_parse_message(PARSE_ERROR, info, current_line);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise, we will just return the node that we got
		return variable;

	//We can also see a constant
	} else if (lookahead.tok == CONSTANT){
		//Again put the token back
		push_back_token(fl, lookahead);

		//Call the constant rule to grab the constant node
		generic_ast_node_t* constant_node = constant(fl);

		//Whether it's null or not, we'll just give it back to the caller to handle
		return constant_node;

	//This is the case where we are putting the expression
	//In parens
	} else if (lookahead.tok == L_PAREN){
		//We'll push it up to the stack for matching
		push(grouping_stack, lookahead);

		//We are now required to see a valid expression
		generic_ast_node_t* expr = expression(fl);

		//If it's an error, just give the node back
		if(expr->CLASS == AST_NODE_CLASS_ERR_NODE){
			return expr;
		}

		//Otherwise it worked, but we're still not done. We now must see the R_PAREN and
		//match it with the accompanying L_PAREN
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case here
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression", parser_line_num);
			num_errors++;
			return 0;
		}

		//Another fail case, if they're unmatched
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
			num_errors++;
			return 0;
		}

		//If we make it here, return the expression node
		return expr;
	
	//Otherwise, if we see an @ symbol, we know it's a function call
	} else if(lookahead.tok == AT){
		//We will let this rule handle the function call
		generic_ast_node_t* func_call = function_call(fl);

		//Whatever it ends up being, we'll just return it
		return func_call;

	//Generic fail case
	} else {
		sprintf(info, "Expected identifier, constant or (<expression>), but got %s", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return 0;
	}
}


/**
 * An assignment expression can decay into a conditional expression or it
 * can actually do assigning. There is no chaining in Ollie language of assignments. There are two
 * options for treenodes here. If we see an actual assignment, there is a special assignment node
 * that will be made. If not, we will simply pass the parent along
 *
 * BNF Rule: <assignment-expression> ::= <conditional-expression> 
 * 									   | asn <unary-expression> := <conditional-expression>
 *
 * TODO TYPE CHECKING REQUIRED
 */
static u_int8_t assignment_expression(FILE* fl, generic_ast_node_t* parent_node){
	//Info array for error printing
	char info[2000];
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//Status var
	u_int8_t status = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see an assign keyword, we know that 
	//we're just passing through to a conditional expression
	if(lookahead.tok != ASN){
		//Put the token back
		push_back_token(fl, lookahead);

		//Pass through with the exact same parent node
		status = conditional_expression(fl, parent_node);

		//Not a "leaf error", just bail out
		if(status == 0){
			return 0;
		}

		//Otherwise it all worked here so
		return 1;
	}

	//If we make it here however, that means that we did see the assign keyword. Since
	//this is the case, we'll make a new assignment node and take the appropriate actions here 
	generic_ast_node_t* asn_expr_node = ast_node_alloc(AST_NODE_CLASS_ASNMNT_EXPR);	

	//This will be a child of whomever the parent is
	add_child_node(parent_node, asn_expr_node);

	//Now we must see a valid unary expression. The unary expression's parent
	//will itself be the assignment expression node
	
	//We'll let this rule handle it
	status = unary_expression(fl, asn_expr_node);

	//Fail out here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid left hand side given to assignment expression", current_line);
		return 0;
	}

	//Now we are required to see the := terminal
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Fail case here
	if(lookahead.tok != COLONEQ){
		sprintf(info, "Expected := symbol in assignment expression, instead got %s", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return 0;
	}

	//Now that we're here we must see a valid conditional expression
	status = conditional_expression(fl, asn_expr_node);

	//Fail case here
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid right hand side given to assignment expression", current_line);
		num_errors++;
		return 0;
	}

	//Otherwise it all worked so
	return 0;
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
 * 						    	| < <type-specifier> > <unary-expression>
 */
static generic_ast_node_t* cast_expression(FILE* fl){
	//The lookahead token
	Lexer_item lookahead;

	//If we first see an angle bracket, we know that we are truly doing
	//a cast. If we do not, then this expression is just a pass through for
	//a unary expression
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If it's not the <, put the token back and just return the unary expression
	if(lookahead.tok != L_THAN){
		push_back_token(fl, lookahead);

		//Let this handle it
		return unary_expression(fl);
	}
	//Push onto the stack for matching
	push(grouping_stack, lookahead);

	//Grab the type specifier
	generic_ast_node_t* type_spec = type_specifier(fl);

	//If it's an error, we'll print and propagate it up
	if(type_spec->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given to cast expression", parser_line_num);
		num_errors++;
		//It is the error, so we can return it
		return type_spec;
	}

	//We now have to see the closing braces that we need
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't see a match
	if(lookahead.tok != G_THAN){
		print_parse_message(PARSE_ERROR, "Expected closing > at end of cast", parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Make sure we match
	if(pop(grouping_stack).tok != L_THAN){
		print_parse_message(PARSE_ERROR, "Unmatched angle brackets given to cast statement", parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we have to see a valid unary expression. This is our last potential fail case in the chain
	//The unary expression will handle this for us
	generic_ast_node_t* right_hand_unary = unary_expression(fl);

	//If it's an error we'll jump out
	if(right_hand_unary->CLASS == AST_NODE_CLASS_ERR_NODE){
		return right_hand_unary;
	}

	//Now if we make it here, we know that type_spec is actually valid
	//We'll now allocate a cast expression node
	generic_ast_node_t* cast_node = ast_node_alloc(AST_NODE_CLASS_CAST_EXPR);
	
	//This node will have a first child as the actual type node
	add_child_node(cast_node, type_spec);

	//Store the type information for faster retrieval later
	((cast_expr_ast_node_t*)(cast_node->node))->casted_type = ((type_spec_ast_node_t*)(type_spec->node))->type_record->type;

	//We'll now add the unary expression as the right node
	add_child_node(cast_node, right_hand_unary);

	//Finally, we're all set to go here, so we can return the root reference
	return cast_node;
}


/**
 * A multiplicative expression can be chained and decays into a cast expression. This method
 * will return a pointer to the root of the subtree that is created by it, whether that subtree
 * originated here or not
 *
 * BNF Rule: <multiplicative-expression> ::= <cast-expression>{ (* | / | %) <cast-expression>}*
 */
static generic_ast_node_t* multiplicative_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid cast expression expression
	generic_ast_node_t* sub_tree_root = cast_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any *'s or %'s or /, we just add 
	//this node in as the child and move along. But if we do see * or % or / symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a relational operators(* or % or /) 
	while(lookahead.tok == MOD || lookahead.tok == STAR || lookahead.tok == F_SLASH){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid cast expression again
		right_child = cast_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the token we need, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * Additive expressions can be chained like some of the other expressions that we see below. It is guaranteed
 * to return a pointer to a sub-tree, whether that subtree is created here or elsewhere.
 *
 * BNF Rule: <additive-expression> ::= <multiplicative-expression>{ (+ | -) <multiplicative-expression>}*
 */
static generic_ast_node_t* additive_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid multiplicative expression
	generic_ast_node_t* sub_tree_root = multiplicative_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any +'s or -'s, we just add 
	//this node in as the child and move along. But if we do see + or - symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a relational operators(+ or -) 
	while(lookahead.tok == PLUS || lookahead.tok == MINUS){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid multiplicative expression again
		right_child = multiplicative_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the token we need, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A shift expression cannot be chained, so no recursion is needed here. It decays into an additive expression.
 * Just like other expression rules, a shift expression will return a subtree root, whether that subtree is 
 * rooted here or elsewhere
 *
 * BNF Rule: <shift-expression> ::= <additive-expression> 
 *								 |  <additive-expression> << <additive-expression> 
 *								 |  <additive-expression> >> <additive-expression>
 */
static generic_ast_node_t* shift_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid additive expression
	generic_ast_node_t* sub_tree_root = additive_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any shift operators, we just add 
	//this node in as the child and move along. But if we do see shift operator symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a relational operators(== or !=) 
	if(lookahead.tok == L_SHIFT || lookahead.tok == R_SHIFT){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid additive expression again
		right_child = additive_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

	} else {
		//Otherwise just push the token back
		push_back_token(fl, lookahead);
	}

	//Once we make it here, the subtree root is either just the shift expression or it is the
	//shift expression rooted at the relational operator

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A relational expression will descend into a shift expression. Ollie language does not allow for
 * chaining in relational expressions, so there will be no while loop like other rules. Just like
 * other expression rules, a relational expression will return a subtree, whether that subtree
 * is made here or elsewhere
 *
 * <relational-expression> ::= <shift-expression> 
 * 						     | <shift-expression> > <shift-expression> 
 * 						     | <shift-expression> < <shift-expression> 
 * 						     | <shift-expression> >= <shift-expression> 
 * 						     | <shift-expression> <= <shift-expression>
 */
static generic_ast_node_t* relational_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid shift expression
	generic_ast_node_t* sub_tree_root = shift_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any relational operators, we just add 
	//this node in as the child and move along. But if we do see relational operator symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a relational operators(== or !=) 
	if(lookahead.tok == G_THAN || lookahead.tok == G_THAN_OR_EQ
	  || lookahead.tok == L_THAN || lookahead.tok == L_THAN_OR_EQ){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid shift again
		right_child = shift_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

	} else {
		//Otherwise just push the token back
		push_back_token(fl, lookahead);
	}

	//Once we make it here, the subtree root is either just the shift expression or it is the
	//shift expression rooted at the relational operator

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An equality expression can be chained and descends into a relational expression. It will
 * always return a pointer to the subtree, whether that subtree is made here or elsewhere
 *
 * BNF Rule: <equality-expression> ::= <relational-expression>{ (==|!=) <relational-expression> }*
 */
static generic_ast_node_t* equality_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid relational expression
	generic_ast_node_t* sub_tree_root = relational_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any =='s or !='s, we just add 
	//this node in as the child and move along. But if we do see == or != symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a relational operators(== or !=) 
	while(lookahead.tok == NOT_EQUALS || lookahead.tok == D_EQUALS){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid relational expression again
		right_child = relational_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An and-expression descends into an equality expression and can be chained. This function
 * will always return a pointer to the root of the subtree, whether that subtree is made here or
 * at a rule lower down on the tree
 *
 * BNF Rule: <and-expression> ::= <equality-expression>{& <equality-expression>}* 
 */
static generic_ast_node_t* and_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid equality expression
	generic_ast_node_t* sub_tree_root = equality_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ^'s, we just add 
	//this node in as the child and move along. But if we do see ^ symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a single and(&) 
	while(lookahead.tok == AND){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid equality expression again
		right_child = equality_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An exclusive or expression can be chained, and descends into an and-expression. It will always return
 * a node pointer to the root of the subtree, whether that subtree is made here or in a rule lower down
 * the chain
 *
 * BNF Rule: <exclusive-or-expression> ::= <and-expression>{^ <and-expression}*
 */
static generic_ast_node_t* exclusive_or_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid and expression
	generic_ast_node_t* sub_tree_root = and_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ^'s, we just add 
	//this node in as the child and move along. But if we do see ^ symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a single xor(^)
	while(lookahead.tok == CARROT){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid and expression again
		right_child = and_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An inclusive or expression will always return a reference to the root node of it's subtree. That node
 * could be an operator or it could be a passthrough
 *
 * BNF rule: <inclusive-or-expression> ::= <exclusive-or-expression>{ | <exclusive-or-expression>}*
 */
static generic_ast_node_t* inclusive_or_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid exclusive or expression
	generic_ast_node_t* sub_tree_root = exclusive_or_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any |'s, we just add 
	//this node in as the child and move along. But if we do see | symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a single or(|)
	while(lookahead.tok == OR){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid exclusive or expression again
		right_child = exclusive_or_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A logical-and-expression will always return a reference to the root node of its subtree. That
 * root node can very well be an operator or it could just be a pass through
 *
 * BNF Rule: <logical-and-expression> ::= <inclusive-or-expression>{&&<inclusive-or-expression>}*
 */
static generic_ast_node_t* logical_and_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid inclusive or expression
	generic_ast_node_t* sub_tree_root = inclusive_or_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any &&'s, we just add 
	//this node in as the child and move along. But if we do see && symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a double and 
	while(lookahead.tok == DOUBLE_AND){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid inclusive or expression again
		right_child = inclusive_or_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(fl, lookahead);

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A logical or expression can be chained together as many times as we want, and
 * descends into a logical and expression
 *
 * A logical or expression will be a parent and a child in the AST
 *
 * BNF Rule: <logical-or-expression> ::= <logical-and-expression>{||<logical-and-expression>}*
 */
static u_int8_t logical_or_expression(FILE* fl, generic_ast_node_t* parent_node){
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a logical and expression
	generic_ast_node_t* sub_tree_root = logical_and_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		return 0;
	}
	
	//There are now two options. If we do not see any ||'s, we just add 
	//this node in as the child and move along. But if we do see || symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a double or
	while(lookahead.tok == DOUBLE_OR){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		((binary_expr_ast_node_t*)(sub_tree_root->node))->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid logical and expression again
		right_child = logical_and_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			return 0;
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "DOUBLE OR" token, so we are done. We'll put
	//the token back and add this in as a subtree of the parent
	push_back_token(fl, lookahead);

	//Add it in as a child
	add_child_node(parent_node, sub_tree_root);

	return 1;
}


/**
 * A conditional expression is simply used as a passthrough for a logical or expression,
 * but some important checks may be done here so we'll use it
 *
 * BNF Rule: <conditional-expression> ::= <logical-or-expression>
 */
static u_int8_t conditional_expression(FILE* fl, generic_ast_node_t* parent_node){
	//Status var
	u_int8_t status = 0;

	//We'll now hand the entire thing off to the logical-or-expression node
	//The expression node that we made here is the parent
	status = logical_or_expression(fl, parent_node);

	//Something failed, but we don't have a leaf error so just leave
	if(status == 0){
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


/**
 * A type address specifier allows us to specify that a type is actually an address(&) or some kind of array of these types
 * There is no limit to how deep the array or address manipulation can go, so this rule is recursive. This rule actively
 * modifies the current_record type record that it has, updating it to support whatever type we have
 *
 * In the interest of memory safety, ollie language requires array bounds for static arrays to be known at compile time
 *
 * BNF Rule: {type-address-specifier} ::= [<constant>]{type-address-specifier}
 * 										| &{type-address-specifier}
 * 										| epsilon
 */
static u_int8_t type_address_specifier(FILE* fl, generic_ast_node_t* type_specifier, generic_type_t** current_type){
	//Status checker
	u_int8_t status = 0;
	//Lookahead token
	Lexer_item lookahead;
	//A node that we'll be adding to the parent if we see something
	generic_ast_node_t* node;

	//Let's see what we have here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//What type do we have
	//Single and sign(&) means pointer
	if(lookahead.tok == AND){
		//Allocate it
		node = ast_node_alloc(AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER);

		//Copy this in for storage
		strcpy(((type_address_specifier_ast_node_t*)(node->node))->address_specifer, "&");

		//This node will always be the child of a type specifier node
		add_child_node(type_specifier, node);

		//We'll make a new pointer type that points back to the original type
		*current_type = create_pointer_type(*current_type, parser_line_num);

		//We'll see if we need to keep going
		return type_address_specifier(fl, type_specifier, current_type);

	} else if(lookahead.tok == L_BRACKET){
		//Push the L_Bracket onto the stack for matching
		push(grouping_stack, lookahead);

		//Allocate it
		node = ast_node_alloc(AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER);

		//Copy this in for storage
		strcpy(((type_address_specifier_ast_node_t*)(node->node))->address_specifer, "[]");

		//This node will always be the child of a type specifier node
		add_child_node(type_specifier, node);

		status = constant(fl, type_specifier);

		//TODO FINISH

	//This is our epsilon area, we'll just put it back and leave
	} else {
		push_back_token(fl, lookahead);
		return 1;
	}
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
	//Lookahead var
	Lexer_item lookahead;

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
	symtab_type_record_t* current_type_record = lookup_type(type_symtab, type_name);

	//This is a "leaf-level" error
	if(current_type_record == NULL){
		sprintf(info, "Type with name: \"%s\" does not exist in the current scope.", type_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		return 0;
	}

	//Let's see where we go from here
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok == AND || lookahead.tok == L_BRACKET){
		//Now if we make it here, we know that the type exists in the system, and we have a record of it
		//in our hands. We can now optionally see some type-address-specifiers. These take the form of
		//array brackets or address operators(&)
		//
		//The type-address-specifier function works uniquely compared to other functions. It will actively
		//modify the type that we have currently active. When it's done, our "current_type" reference should
		//in theory be fully done with arrays
		//
		//Just like before, this node is the child of the type-spec-node

		//We'll first push the token back
		push_back_token(fl, lookahead);

		//Now we expect to have some new types made
		generic_type_t* current_type = current_type_record->type;

		//We'll now let this do it's thing. By the time we come back, current_type
		//will automagically be the complete type
		status = type_address_specifier(fl, type_spec_node, &current_type);

		//Non-leaf error here, no need to print anything
		if(status == 0){
			return 0;
		}
		
		//Let's now search to see if this type name has ever appeared before. If it has, there
		//is no issue with that. Duplicated pointer and array types are of no concern, as they are
		//universal
		current_type_record = lookup_type(type_symtab, current_type->type_name);

		//If we actually found it, we'll just reuse that same record
		if(current_type_record != NULL){
			//We no longer need this type
			destroy_type(current_type);
			//Assign this and get out
			((type_spec_ast_node_t*)(type_spec_node->node))->type_record = current_type_record;
			return 1;
		//Otherwise we'll make a totally new type record
		} else {
			current_type_record = create_type_record(current_type);
			//Put into symtab
			insert_type(type_symtab, current_type_record);

			//Assign this and get out
			((type_spec_ast_node_t*)(type_spec_node->node))->type_record = current_type_record;

			return 1;
		}

	} else {
		//If we make it here, there will be no type modifications or potential new types made. The pointer
		//to the type record that we already have is actually completely valid, and as such we'll just
		//stash it and get out

		//Put whatever we saw back
		push_back_token(fl, lookahead);

		//Store the reference to the type that we have here
		((type_spec_ast_node_t*)(type_spec_node->node))->type_record = current_type_record;

		return 1;
	}
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
	//For any needed error printing
	char info[2000];

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

	//We are now required to see a valid identifier for the function
	status = identifier(fl, parameter_decl_node);

	//Again if it's bad bail
	if(status == 0){
		num_errors++;
		return 0;
	}

	//Once we get here, we have actually seen an entire valid parameter 
	//declaration. It is now incumbent on us to store it in the variable 
	//symbol table

	//Define a cursor for tree walking
	generic_ast_node_t* cursor = parameter_decl_node;

	//We'll now walk the subtree that parameter-decl has in order. We expect
	//to first see the type specifier
	cursor = cursor->first_child;
	
	//Fatal error, debug message for dev only
	if(cursor->CLASS != AST_NODE_CLASS_TYPE_SPECIFIER){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Expected type specifier in parameter declaration", parser_line_num);
		return 0;
	}

	//Grab the type record reference
	symtab_type_record_t* parameter_type = ((type_spec_ast_node_t*)(cursor->node))->type_record;

	//Now walk to the next child. If all is correct, this should be an identifier
	cursor = cursor->next_sibling;

	//Again this needs to be an identifier
	if(cursor->CLASS != AST_NODE_CLASS_IDENTIFER){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Expected identifier in parameter declaration", parser_line_num);
		return 0;
	}

	//Grab the ident record
	identifier_ast_node_t* ident = cursor->node;

	//Now we must perform all of our symtable checks. Parameters may not share names with types, functions or variables
	symtab_function_record_t* found_function = lookup_function(function_symtab, ident->identifier); 

	if(found_function != NULL){
		sprintf(info, "A function with name \"%s\" has already been defined. First defined here:", found_function->func_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_function_name(found_function);
		num_errors++;
		return 0;
	}

	//Check for duplicated variables
	symtab_variable_record_t* found_variable = lookup_variable(variable_symtab, ident->identifier); 

	if(found_variable != NULL){
		sprintf(info, "A variable with name \"%s\" has already been defined. First defined here:", found_variable->var_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_variable_name(found_variable);
		num_errors++;
		return 0;
	}

	//Check for duplicated type names
	symtab_type_record_t* found_type = lookup_type(type_symtab, ident->identifier); 

	if(found_type != NULL){
		sprintf(info, "A type with name \"%s\" has already been defined. First defined here:", found_type->type->type_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_type_name(found_type);
		num_errors++;
		return 0;
	}

	//If we make it here we've passed all of the checks

	//Now we have the identifier and the type, we can build our record
	symtab_variable_record_t* param = create_variable_record(ident->identifier, STORAGE_CLASS_NORMAL);
	//Assign the parameter type
	param->type = parameter_type->type;
	//Constant status
	param->is_constant = is_constant;
	//It is a function parameter
	param->is_function_paramater = 1;

	//Insert into the symbol table
	insert_variable(variable_symtab, param);

	//And, we'll hold a reference to it inside of the node as well
	((param_decl_ast_node_t*)(parameter_decl_node->node))->param_record = param;

	//All went well
	return 1;
}


/**
 * Optional repetition allowed with our parameter list. Merely a passthrough function,
 * no nodes created directly
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
		//Whatever it was, we don't want it so put it back
		push_back_token(fl, lookahead);
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
 * A parameter list will always be the child of a function node. It is important to note
 * that the <parameter-declaration> function is responsible for verifying and storing
 * each individual parameter in the parameter list, this function does not perform
 * that duty
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

	//Let's now create the parameter list node and add it into the tree
	generic_ast_node_t* param_list_node = ast_node_alloc(AST_NODE_CLASS_PARAM_LIST);

	//This will be the child of the function node
	add_child_node(parent, param_list_node);

	//There are two options that we have here. We can have an entirely blank
	//parameter list. 
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see an R_PAREN, it's blank so we'll just leave
	if(lookahead.tok == R_PAREN){
		//Put it back for checking by the caller
		push_back_token(fl, lookahead);
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
 * A declare statement is always the child of an overall declaration statement, so it will
 * be added as the child of the given parent node. A declare statement also performs all
 * needed type/repetition checks 
 *
 * BNF Rule: <declare-statement> ::= declare {constant}? {<storage-class-specifier>}? <type-specifier> <declarator>;
 */
static u_int8_t declare_statement(FILE* fl, generic_ast_node_t* parent_node){

}


/**
 * A let statement is always the child of an overall declaration statement. Like a declare statement, it also
 * performs type checking and inference and all needed symbol table manipulation
 *
 * BNF Rule: <let-statement> ::= let {constant}? {<storage-class-specifier>}? <type-specifier> <declarator> := <initializer>;
 */
static u_int8_t let_statement(FILE* fl, generic_ast_node_t* parent_node){

}


/**
 * A define statement allows users to define complex types like enumerateds and constructs and give them aliases
 * inline(there is also a separate aliasing feature). Just like any other declaration, this function performs 
 * all type checking and name checking and symbol table manipulation. It is also always the child of some given
 * node
 *
 * BNF Rule: <define-statement> ::= define <complex-type-definer> {as <alias-identifer>}?;
 */
static u_int8_t define_statement(FILE* fl, generic_ast_node_t* parent_node){

}


/**
 * An alias statement allows us to redefine any currently defined type as some other type. It is probably the
 * simplest of any of these rules, but it still performs all type checking and symbol table manipulation. It is
 * always the child of a parent node
 *
 * BNF Rule: *<alias-statement> ::= alias <type-specifier> as <identifier>;
 */
static u_int8_t alias_statement(FILE* fl, generic_ast_node_t* parent_node){

}


/**
 * A declaration is a pass through rule that does not itself initialize a node. Instead, it will pass down to
 * the appropriate rule here and let them initialize the rule. The declaration itself does have a parent node,
 * so it will need to pass that parent node down through to these rules here
 *
 * <declaration> ::= <declare-statement> 
 * 				   | <let-statement> 
 * 				   | <define-statement> 
 * 				   | <alias-statement>
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
 * Handle the case where we declare a function. A function will always be one of the children of a declaration
 * partition
 *
 * NOTE: We have already consumed the FUNC keyword by the time we arrive here, so we will not look for it in this function
 *
 * BNF Rule: <function-definition> ::= func {:<function-specifier>}? <identifer> ({<parameter-list>}?) -> <type-specifier> <compound-statement>
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
	//We will define a cursor that we will use to walk the children of the function node
	//as the function's subtree is built. This will allow us to incrementally move up
	//as opposed to doing it all at once
	generic_ast_node_t* cursor = NULL;

	//What is the function's storage class? Normal by default
	STORAGE_CLASS_T storage_class;

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
		
		//At this point we can initialize the cursor
		cursor = function_node->first_child;

		//Largely for dev usage
		if(cursor == NULL || cursor->CLASS != AST_NODE_CLASS_FUNC_SPECIFIER){
			print_parse_message(PARSE_ERROR, "Fatal internal parse error. Expected function specifier node as child", current_line);
			return 0;
		}

		//Also stash this for later use
		storage_class = ((func_specifier_ast_node_t*)(cursor->node))->function_storage_class;

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

	//Now we can actually grab the identifier out. This next sibling should be an ident
	cursor = cursor->next_sibling;
	
	//For dev use
	if(cursor == NULL || cursor->CLASS != AST_NODE_CLASS_IDENTIFER){
		print_parse_message(PARSE_ERROR, "Fatal internal parse error. Expected identifier node as next sibling", current_line);
		return 0;
	}

	//This in theory should be an ident node
	identifier_ast_node_t* ident = cursor->node;

	//Let's now do all of our checks for duplication before we go any further. This can
	//save us time if it ends up being bad
	
	//Now we must perform all of our symtable checks. Parameters may not share names with types, functions or variables
	symtab_function_record_t* found_function = lookup_function(function_symtab, ident->identifier); 

	if(found_function != NULL){
		sprintf(info, "A function with name \"%s\" has already been defined. First defined here:", found_function->func_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_function_name(found_function);
		num_errors++;
		return 0;
	}

	//Check for duplicated variables
	symtab_variable_record_t* found_variable = lookup_variable(variable_symtab, ident->identifier); 

	if(found_variable != NULL){
		sprintf(info, "A variable with name \"%s\" has already been defined. First defined here:", found_variable->var_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_variable_name(found_variable);
		num_errors++;
		return 0;
	}

	//Check for duplicated type names
	symtab_type_record_t* found_type = lookup_type(type_symtab, ident->identifier); 

	if(found_type != NULL){
		sprintf(info, "A type with name \"%s\" has already been defined. First defined here:", found_type->type->type_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_type_name(found_type);
		num_errors++;
		return 0;
	}

	//Now that we know it's fine, we can first create the record. There is still more to add in here, but we can at least start
	//it
	symtab_function_record_t* function_record = create_function_record(ident->identifier, storage_class);
	//Associate this with the function node
	((func_def_ast_node_t*)function_node->node)->func_record = function_record;

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

	//We initialize this scope automatically, even if there is no param list.
	//It will just be empty if this is the case, no big issue
	initialize_variable_scope(variable_symtab);

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

	//Once we make it here, we know that we have a valid param list and valid parenthesis. We can
	//now parse the param_list and store records to it	
	//Advance the cursor to its next sibling
	
	//If it's not null, there is a parameter list
	if(cursor->next_sibling != NULL){
		//Advance the cursor
		cursor = cursor->next_sibling;

		//Some very weird error here
		if(cursor->CLASS != AST_NODE_CLASS_PARAM_LIST){
			print_parse_message(PARSE_ERROR, "Fatal internal parse error. Expected parameter list node as next sibling", current_line);
			return 0;
		}

		//The actual parameters are children of the param list cursor
		generic_ast_node_t* param_cursor = cursor->first_child;

		//Now we'll walk the param list
		while(param_cursor != NULL){
			function_record->func_params[function_record->number_of_params] = ((param_decl_ast_node_t*)(param_cursor->node))->param_record;
			function_record->number_of_params++;
			
			//If this happens get out
			if(function_record->number_of_params > 6){
				print_parse_message(PARSE_ERROR, "Ollie language restricts parameter numbers to 6 due to register constraints", current_line);
				num_errors++;
				return 0;
			}
			//Move it up
			param_cursor = param_cursor->next_sibling;
		}
	}
	
	//Once we get down here, the cursor should be precisely poised
	
	//Semantics here, we now must see a valid arrow symbol
	lookahead = get_next_token(fl, &parser_line_num);

	//If it isn't an arrow, we're out of here
	if(lookahead.tok != ARROW){
		print_parse_message(PARSE_ERROR, "Arrow(->) required after parameter-list in function", parser_line_num);
		num_errors++;
		return 0;
	}

	//Now if we get here, we must see a valid type specifier
	//The parent of this will be the function node
	status = type_specifier(fl, function_node);

	//If we failed, bail out
	if(status == 0){
		print_parse_message(PARSE_ERROR, "Invalid return type given to function. All functions, even void ones, must have an explicit return type", parser_line_num);
		num_errors++;
		return 0;
	}

	//Next sibling must be a type_specifier node
	cursor = cursor->next_sibling;

	//Dev uses only
	if(cursor == NULL || cursor->CLASS != AST_NODE_CLASS_TYPE_SPECIFIER){
		print_parse_message(PARSE_ERROR, "Fatal internal parse error. Expected type specifier node as next sibling", parser_line_num);
		return 0;
	}

	//Grab the type record. A reference to this will be stored in the function symbol table
	symtab_type_record_t* type = ((type_spec_ast_node_t*)(cursor->node))->type_record;

	//Store the return type
	function_record->return_type = type;

	//Once we get here, we must see a valid compound statement. The function node
	//will be considered the parent of the compound statement
	status = compound_statement(fl, function_node);

	//Not a leaf error, we can just leave
	if(status == 0){
		return 0;
	}

	//Finally, we'll put the function into the symbol table
	//since we now know that everything worked
	insert_function(function_symtab, function_record);

	//Finalize the variable scope for the parameter list
	finalize_variable_scope(variable_symtab);

	//All good so we can get out
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
	//We consume the function token here, NOT in the function rule
	if(lookahead.tok == FUNC){
		//Otherwise our status is just whatever the function returns
		status = function_definition(fl, parent_node);

		//Something failed
		if(status == 0){
			print_parse_message(PARSE_ERROR, "Invalid function definition", current_line);
			return 0;
		}

	//Otherwise it must be a declaration
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
