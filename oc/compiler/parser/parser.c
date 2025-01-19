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
 * this will be called, we can not do any symbol table validation here. 
 *
 * BNF "Rule": <identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* identifier(FILE* fl){
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
	generic_ast_node_t* ident_node = ast_node_alloc(AST_NODE_CLASS_IDENTIFIER); //Add the identifier into the node itself
	//Copy the string we got into it
	strcpy(((identifier_ast_node_t*)(ident_node->node))->identifier, lookahead.lexeme);

	//Return our reference to the node
	return ident_node;
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
 * BNF Rule: <function-call> ::= @<identifier>({<conditional-expression>}?{, <conditional_expression>}*)
 */
static generic_ast_node_t* function_call(FILE* fl){
	//For generic error printing
	char info[2000];
	//The current line num
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	Lexer_item lookahead;
	//A nicer reference that we'll keep to the function record
	symtab_function_record_t* function_record;
	//We'll also keep a nicer reference to the function name
	char* function_name;
	//The number of parameters that we've seen
	u_int8_t num_params = 0;
	//The number of parameters that the function actually takes
	u_int8_t function_num_params;
	
	//First grab the ident node
	generic_ast_node_t* ident = identifier(fl);

	//We have a general error-probably will be quite uncommon
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Non identifier provided as function call", parser_line_num);
		num_errors++;
		//We'll let the node propogate up
		return ident;
	}

	//Grab the function name out for convenience
	function_name = ((identifier_ast_node_t*)(ident->node))->identifier;

	//Let's now look up the function name in the function symtab
	function_record = lookup_function(function_symtab, function_name);

	//Important check here--if this function record does not exist, it means the user is trying to 
	//call a nonexistent function
	if(function_record == NULL){
		sprintf(info, "Function \"%s\" is being called before definition", function_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		//Return the error node and get out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we can grab out some info for convenience
	function_num_params = function_record->number_of_params;
	//For convenience we can also keep a reference to the func params list
	parameter_t* func_params = function_record->func_params;

	//If we make it here, we know that our function actually exists. We can now create
	//the appropriate node that will hold all of our data about it
	//It is also now safe enough for us to allocate the function node
	generic_ast_node_t* function_call_node = ast_node_alloc(AST_NODE_CLASS_FUNCTION_CALL);

	//The function IDENT will be the first child of this node
	add_child_node(function_call_node, ident);

	//Add the inferred type in for convenience as well
	((function_call_ast_node_t*)(function_call_node->node))->inferred_type = function_record->return_type;
	
	//We now need to see a left parenthesis for our param list
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out here
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected on function call", parser_line_num);
		num_errors++;
		//Send this error node up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//A node to hold our current parameter
	generic_ast_node_t* current_param;

	//Refresh the lookahead token
	lookahead = get_next_token(fl, &parser_line_num);

	//So long as we don't see the R_PAREN we aren't done
	while(1){
		//Parameters are in the form of a conditional expression
		current_param = conditional_expression(fl);

		//We now have an error of some kind
		if(current_param->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Bad parameter passed to function call", current_line);
			num_errors++;
			//Return the error node -- it will propogate up the chain
			return current_param;
		}

		//Otherwise it was fine. We'll first record that we saw one more parameter
		num_params++;

		//If we're exceeding the number of parameters, we'll fail out
		if(num_params > function_num_params){
			sprintf(info, "Function \"%s\" expects %d params, was given %d. First declared here:", function_name, function_num_params, num_params);
			print_parse_message(PARSE_ERROR, info, current_line);
			//Print out the actual function record as well
			print_function_name(function_record);
			num_errors++;
			//Return the error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We can now safely add this into the function call node as a child. In the function call node, 
		//the parameters will appear in order from left to right
		add_child_node(function_call_node, current_param);
		
		//Refresh the token
		lookahead = get_next_token(fl, &parser_line_num);

		//Two options here, we can either see a COMMA or an R_PAREN
		//If it's an R_PAREN we're done
		if(lookahead.tok == R_PAREN){
			break;
		}

		//Otherwise it must be a comma. If it isn't we have a failure
		if(lookahead.tok != COMMA){
			print_parse_message(PARSE_ERROR, "Commas must be used to separate parameters in function call", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	}

	//Once we get here, we do need to finally verify that the closing R_PAREN matched the opening one
	if(pop(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected in function call", parser_line_num);
		num_errors++;
		//Return the error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise, if we make it here, we're all good to return the function call node
	return function_call_node;
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
		generic_ast_node_t* ident = identifier(fl);

		//If there was a failure of some kind, we'll allow it to propogate up
		if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
			//Send the error up the chain
			return ident;
		}

		//Grab this out for convenience
		char* var_name = ((identifier_ast_node_t*)(ident->node))->identifier;

		//Now we will look this up in the variable symbol table
		symtab_variable_record_t* found = lookup_variable(variable_symtab, var_name);

		//We now must see a variable that was intialized. If it was not
		//initialized, then we have an issue
		if(found == NULL){
			sprintf(info, "Variable \"%s\" has not been declared", var_name);
			print_parse_message(PARSE_ERROR, info, current_line);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise, we will just return the node that we got
		return ident;

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
 * that will be made. If not, we will simply pass the parent along. An assignment expression will return
 * a reference to the subtree created by it
 *
 * BNF Rule: <assignment-expression> ::= <conditional-expression> 
 * 									   | asn <unary-expression> := <conditional-expression>
 *
 * TODO TYPE CHECKING REQUIRED
 */
static generic_ast_node_t* assignment_expression(FILE* fl){
	//Info array for error printing
	char info[2000];
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see an assign keyword, we know that 
	//we're just passing through to a conditional expression
	if(lookahead.tok != ASN){
		//Put the token back
		push_back_token(fl, lookahead);

		//Simply let the conditional expression rule handle it
		return conditional_expression(fl);
	}

	//If we make it here however, that means that we did see the assign keyword. Since
	//this is the case, we'll make a new assignment node and take the appropriate actions here 
	generic_ast_node_t* asn_expr_node = ast_node_alloc(AST_NODE_CLASS_ASNMNT_EXPR);	

	//Now we must see a valid unary expression. The unary expression's parent
	//will itself be the assignment expression node
	
	//We'll let this rule handle it
	generic_ast_node_t* left_hand_unary = unary_expression(fl);

	//Fail out here
	if(left_hand_unary->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid left hand side given to assignment expression", current_line);
		//Return the erroneous node as we fail up the tree
		return left_hand_unary;
	}

	//Otherwise it worked, so we'll add it in as the left child
	add_child_node(asn_expr_node, left_hand_unary);
	//TODO add more with types

	//Now we are required to see the := terminal
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Fail case here
	if(lookahead.tok != COLONEQ){
		sprintf(info, "Expected := symbol in assignment expression, instead got %s", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Return a special kind of error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we're here we must see a valid conditional expression
	generic_ast_node_t* conditional = conditional_expression(fl);

	//Fail case here
	if(conditional->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid right hand side given to assignment expression", current_line);
		num_errors++;
		//The conditional is already an error, so we'll just return it
		return conditional;
	}

	//Otherwise we know it worked, so we'll add the conditional in as the right child
	add_child_node(asn_expr_node, conditional);

	//Return the reference to the overall node
	return asn_expr_node;
}


/**
 * A construct accessor is used to access a construct either on the heap of or on the stack.
 * Like all rules, it will return a reference to the root node of the tree that it created
 *
 * A constructor accessor node will be a subtree with the parent holding the actual operator
 * and its child holding the variable identifier
 *
 * We will expect to see the => or : here
 *
 * BNF Rule: <construct-accessor> ::= => <variable-identifier> 
 * 								    | : <variable-identifier>
 */
static generic_ast_node_t* construct_accessor(FILE* fl){
	//Freeze the current line
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	Lexer_item lookahead;

	//We'll first grab whatever token that we have here
	lookahead = get_next_token(fl, &parser_line_num);

	//This would be incredibly bizarre, as we know that they are already here
	if(lookahead.tok != ARROW_EQ && lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Fatal internal parser error at construct accessor", parser_line_num);
		num_errors++;
		//Error out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Otherwise we'll now make the node here
	generic_ast_node_t* const_access_node = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_ACCESSOR);
	//Put the token in to show what we have
	((construct_accessor_ast_node_t*)(const_access_node->node))->tok = lookahead.tok;

	//Now we are required to see a valid variable identifier. TODO TYPE CHECKING
	generic_ast_node_t* ident = identifier(fl); 

	//For now we're just doing error checking TODO TYPE AND EXISTENCE CHECKING
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Construct accessor could not find valid identifier", current_line);
		num_errors++;
		//It already is an error node so we'll return it
		return ident;
	}

	//Otherwise we know that it worked, so we'll add this guy in as a child of the overall construct
	//accessor
	add_child_node(const_access_node, ident);

	//And now we're all done, so we'll just give back the root reference
	return const_access_node;
}


/**
 * An array accessor represents a request to get something from an array memory region. Like all
 * nodes, an array accessor will return a reference to the subtree that it creates
 *
 * We expect that the caller has given back the [ token for this rule
 *
 * BNF Rule: <array-accessor> ::= [ <conditional-expression> ]
 *
 */
static generic_ast_node_t* array_accessor(FILE* fl){
	//The lookahead token
	Lexer_item lookahead;
	//Freeze the current line
	u_int16_t current_line = parser_line_num;

	//We expect to see the left bracket here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If we didn't see it, that's some weird internal error
	if(lookahead.tok != L_BRACKET){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Array accessor did not see [", current_line);
		num_errors++;
		//Fail out here
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise it all went well, so we'll push this onto the stack
	push(grouping_stack, lookahead);

	//Now we are required to see a valid constant expression representing what
	//the actual index is. TODO TYPE CHECKING NEEDED
	generic_ast_node_t* expr = conditional_expression(fl);

	//If we fail, automatic exit here
	if(expr->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given to array accessor", current_line);
		num_errors++;
		//It's already an error so we'll just return it
		return expr;
	}

	//Otherwise, once we get here we need to check for matching brackets
	lookahead = get_next_token(fl, &parser_line_num);

	//If wedon't see a right bracket, we'll fail out
	if(lookahead.tok != R_BRACKET){
		print_parse_message(PARSE_ERROR, "Right bracket expected at the end of array accessor", parser_line_num);
		num_errors++;
		//Give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We also must check for matching with the brackets
	if(pop(grouping_stack).tok != L_BRACKET){
		print_parse_message(PARSE_ERROR, "Unmatched brackets detected in array accessor", current_line);
		num_errors++;
		//Again give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we've done all of our checks have been done, we can create the actual node
	generic_ast_node_t* array_acc_node = ast_node_alloc(AST_NODE_CLASS_ARRAY_ACCESSOR);
	//TODO encode type info later
	//The conditional expression is a child of this node
	add_child_node(array_acc_node, expr);

	//And now we're done so give back the root reference
	return array_acc_node;
}


/**
 * A postfix expression decays into a primary expression, and there are certain
 * operators that can be chained if context allows. Like all other rules, this rule
 * returns a reference to the root node that it creates
 *
 * As an important note here: We can chain construct accessors and array accessors as much as we wish, 
 * but seeing a plusplus or minusminus is the defintive end of this rule if we see it
 *
 * <postfix-expression> ::= <primary-expression> 
 *						  | <primary-expression> {{<construct-accessor>}*{<array-accessor>*}}* {++|--}?
 */ 
static generic_ast_node_t* postfix_expression(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;

	//No matter what, we have to first see a valid primary expression
	generic_ast_node_t* primary_expr = primary_expression(fl);

	//If we fail, then we're bailing out here
	if(primary_expr->CLASS == AST_NODE_CLASS_ERR_NODE){
		//Just return, no need for any errors here
		return primary_expr;
	}

	//Peek at the next token
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Let's just check if we're able to get out immediately
	if(lookahead.tok != L_BRACKET && lookahead.tok != COLON && lookahead.tok != ARROW_EQ
	   && lookahead.tok != PLUSPLUS && lookahead.tok != MINUSMINUS){
		//Put the token back
		push_back_token(fl, lookahead);
		//Just return what primary expr gave us
		return primary_expr;
	}

	//Otherwise if we make it here, we know that we will have some kind of complex accessor or 
	//post operation, so we can make the node for it
	generic_ast_node_t* postfix_expr_node = ast_node_alloc(AST_NODE_CLASS_POSTFIX_EXPR);
	
	//This node will always have the primary expression as its first child
	add_child_node(postfix_expr_node, primary_expr);

	//Now we can see as many construct accessor and array accessors as we can take
	//TODO TYPE CHECKING NEEDED
	while(lookahead.tok == L_BRACKET || lookahead.tok == COLON || lookahead.tok == ARROW_EQ){
		//Let's see which rule it is
		//We have an array accessor
		if(lookahead.tok == L_BRACKET){
			//Put the token back
			push_back_token(fl, lookahead);
			//Let the array accessor handle it
			generic_ast_node_t* array_acc = array_accessor(fl);
			
			//Let's see if it actually worked
			if(array_acc->CLASS == AST_NODE_CLASS_ERR_NODE){
				print_parse_message(PARSE_ERROR, "Invalid array accessor found in postfix expression", current_line);
				num_errors++;
				//It's already an error, so we'll just give it back
				return array_acc;
			}
			//TODO TYPE CHECKING

			//Otherwise we know it worked. Since this is the case, we can add it as a child to the overall
			//node
			add_child_node(postfix_expr_node, array_acc);

		//Otherwise we have a construct accessor
		} else {
			//Put it back for the rule to deal with
			push_back_token(fl, lookahead);
			//Let's have the rule do it
			generic_ast_node_t* constr_acc = construct_accessor(fl);

			//We have our fail case here
			if(constr_acc->CLASS == AST_NODE_CLASS_ERR_NODE){
				print_parse_message(PARSE_ERROR, "Invalid construct accessor found in postfix expression", current_line);
				num_errors++;
				//It's already an error so send it up
				return constr_acc;
			}

			//TODO TYPE CHECKING
			//Otherwise we know it's good, so we'll add it in as a child
			add_child_node(postfix_expr_node, constr_acc);
		}
		
		//refresh the lookahead for the next iteration
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Now once we get here, we know that we have something that isn't one of accessor rules
	//It could however be postinc/postdec. Let's first see if it isn't
	if(lookahead.tok != PLUSPLUS && lookahead.tok != MINUSMINUS){
		//Put the token back
		push_back_token(fl, lookahead);
		//And we'll give back what we had constructed so far
		return postfix_expr_node;
	}

	//Otherwise if we get here we know that we either have post inc or dec
	//Create the unary operator node
	generic_ast_node_t* unary_post_op = ast_node_alloc(AST_NODE_CLASS_UNARY_OPERATOR);

	//Store the token
	((unary_operator_ast_node_t*)(unary_post_op->node))->unary_operator = lookahead.tok;

	//This will always be the last child of whatever we've built so far
	add_child_node(postfix_expr_node, unary_post_op);

	//Now that we're done, we can get out
	return postfix_expr_node;
}


/**
 * A unary expression decays into a postfix expression. With a unary expression, we are able to
 * apply unary operators and take the size of given types. Like all rules, a unary expression
 * will always return a pointer to the root node of the tree that it creates
 *
 * BNF Rule: <unary-expression> ::= <postfix-expression> 
 * 								  | <unary-operator> <cast-expression> 
 * 								  | typesize(<type-specifier>)
 *
 * Important notes for typesize: It is assumed that the type-specifier node will handle
 * any/all error checking that we need. Type specifier will throw an error if the type has 
 * not been defined
 *
 * For convenience, we will also handle any/all unary operators here
 *
 * BNF Rule: <unary-operator> ::= & 
 * 								| * 
 * 								| + 
 * 								| - 
 * 								| ~ 
 * 								| ! 
 * 								| ++ 
 * 								| --
 */
static generic_ast_node_t* unary_expression(FILE* fl){
	//The lookahead token
	Lexer_item lookahead;

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see the typesize keyword, we are locked in to the typesize rule
	if(lookahead.tok == TYPESIZE){
		//We've seen typesize, so that is our unary operator. To reflect this, we will create 
		//a unary operator node for it
		generic_ast_node_t* unary_op = ast_node_alloc(AST_NODE_CLASS_UNARY_OPERATOR);
		//Assign the typesize operator to this
		((unary_operator_ast_node_t*)(unary_op->node))->unary_operator = TYPESIZE;

		//Now we have to look for the type
		//We must then see left parenthesis
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case here
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after typesize call", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we'll push to the stack for checking
		push(grouping_stack, lookahead);

		//Now we need to see a valid type-specifier. It is important to note that the type
		//specifier requires that a type has actually been defined. If it wasn't defined,
		//then this will return an error node
		generic_ast_node_t* type_spec = type_specifier(fl);

		//If it's an error
		if(type_spec->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Unable to perform cast on undefined type",  parser_line_num);
			num_errors++;
			//It's already an error, so give it back that way
			return type_spec;
		}

		//Otherwise if we get here it actually was defined, so now we'll look for an R_PAREN
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out here if we don't see it
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after type specifer", parser_line_num);
			num_errors++;
			//Create and return the error
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We can also fail if we somehow see unmatched parenthesis
		if(pop(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected in typesize expression", parser_line_num);
			num_errors++;
			//Create and return the error
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise if we make it all the way down here, we are done and can perform final assemble on the node
		generic_ast_node_t* unary_node = ast_node_alloc(AST_NODE_CLASS_UNARY_EXPR);

		//The unary node always has the operator as it's left hand side
		add_child_node(unary_node, unary_op);

		//The next node will always be the type specifier
		add_child_node(unary_node, type_spec);

		//And we are done, so we'll send this out
		return unary_node;

	//Otherwise there is a potential for us to have any other unary operator. If we see any of these, we'll handle them
	//the exact same way
	} else if(lookahead.tok == PLUS || lookahead.tok == PLUSPLUS || lookahead.tok == MINUS || lookahead.tok == MINUSMINUS
		     || lookahead.tok == STAR || lookahead.tok == AND || lookahead.tok == B_NOT || lookahead.tok == L_NOT){

		//We'll first create the unary operateor node for ourselves here
		generic_ast_node_t* unary_op = ast_node_alloc(AST_NODE_CLASS_UNARY_OPERATOR);
		//Assign the typesize operator to this
		((unary_operator_ast_node_t*)(unary_op->node))->unary_operator = lookahead.tok;

		//Following this, we are required to see a valid cast expression
		generic_ast_node_t* cast_expr = cast_expression(fl);

		//Let's check for errors
		if(cast_expr->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If it is bad, we'll just propogate it up the chain
			return cast_expr;
		}

		//Otherwise we know that it worked TODO TYPE CHECKING
		//One we get here, we have both nodes that we need
		generic_ast_node_t* unary_node = ast_node_alloc(AST_NODE_CLASS_UNARY_EXPR);

		//The unary operator always comes first
		add_child_node(unary_node, unary_op);

		//The cast expression will be linked in last
		add_child_node(unary_node, cast_expr);

		//Finally we're all done, so we can just give this back
		return unary_node;
	
	//If we get here we will just put the token back and pass the responsibility on to the
	//postifix expression rule
	} else {
		push_back_token(fl, lookahead);
		return postfix_expression(fl);
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
 * This will always return a reference to the root node of the tree
 *
 * BNF Rule: <logical-or-expression> ::= <logical-and-expression>{||<logical-and-expression>}*
 */
static generic_ast_node_t* logical_or_expression(FILE* fl){
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
		//It's already an error node, so allow it to propogate
		return sub_tree_root;
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
			//It's already an error node, so allow it to propogate
			return right_child;
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

	//Return the reference to the root node
	return sub_tree_root;
}


/**
 * A conditional expression is simply used as a passthrough for a logical or expression,
 * but some important checks may be done here so we'll use it
 *
 * This rule will always return a reference to the root of the subtree it makes
 *
 * BNF Rule: <conditional-expression> ::= <logical-or-expression>
 */
static generic_ast_node_t* conditional_expression(FILE* fl){
	//We'll now hand the entire thing off to the logical-or-expression node
	//and return the reference that it gives us
	return logical_or_expression(fl);
}


/**
 * A construct member is something like a variable declaration. Like all rules in the parser,
 * the construct member will return a reference to the root node of the subtree it creates
 *
 * As a reminder, type specifier will give us an error if the type is not defined
 *
 * BNF Rule: <construct-member> ::= {constant}? <type-specifier> <identifier>
 */
static generic_ast_node_t* construct_member(FILE* fl){
	//The error printing string
	char info[2000];
	//The lookahead token
	Lexer_item lookahead;
	//Is it a constant variable?
	u_int8_t is_constant = 0;

	//Let's first see if it's a constant
	lookahead = get_next_token(fl, &parser_line_num);

	//If it is constant
	if(lookahead.tok == CONSTANT){
		//Then it's constant
		is_constant = 1;
	} else {
		//Otherwise, we'll just put it back
		push_back_token(fl, lookahead);
	}

	//Now we are required to see a valid type specifier
	generic_ast_node_t* type_spec = type_specifier(fl);

	//If this is an error, the whole thing fails
	if(type_spec->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Attempt to use undefined type in construct member", parser_line_num);
		num_errors++;
		//It's already an error, so just send it up
		return type_spec;
	}

	//Otherwise we know that it worked here
	//Now we need to see a valid ident and check it for duplication
	generic_ast_node_t* ident = identifier(fl);	

	//Let's make sure it actually worked
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as construct member name", parser_line_num);
		num_errors++;
		//It's an error, so we'll propogate it up
		return ident;
	}

	//Grab this for convenience
	char* name = ((identifier_ast_node_t*)(ident->node))->identifier;

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now if we finally make it all of the way down here, we are actually set. We'll construct the
	//node that we have and also add it into our symbol table
	
	//We'll first create the symtab record
	symtab_variable_record_t* member_record = create_variable_record(name, STORAGE_CLASS_NORMAL);
	//It is a construct member
	member_record->is_construct_member = 1;
	member_record->line_number = parser_line_num;
	//Store what the type is
	member_record->type = ((type_spec_ast_node_t*)(type_spec)->node)->type_record->type;
	//Store the constant status
	member_record->is_constant = is_constant;
	
	//We can now add this into the symbol table
	insert_variable(variable_symtab, member_record);

	//We can now also construct the entire subtree
	generic_ast_node_t* member_node = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_MEMBER);
	//Store the variable record here
	((construct_member_ast_node_t*)(member_node)->node)->member_var = member_record;

	//The very first child will be the type specifier
	add_child_node(member_node, type_spec);
	//The second child will be the ident node
	add_child_node(member_node, ident);

	//All went well so we can send this up the chain
	return member_node;
}


/**
 * A construct member list holds all of the nodes that themselves represent construct members. Like all
 * other rules, this function returns the root node of the subtree that it creates
 *
 * BNF Rule: <construct-member-list> ::= { <construct-member> ; }*
 */
static generic_ast_node_t* construct_member_list(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Let's first declare the root node. This node will have children that are each construct members
	generic_ast_node_t* member_list = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST);

	//We can see as many construct members as we please here, all delimited by semicols
	do{
		//We must first see a valid construct member
		generic_ast_node_t* member_node = construct_member(fl);

		//If it's an error, we'll fail right out
		if(member_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid construct member declaration", parser_line_num);
			//It's already an error node so just let it propogate
			return member_node;
		}
		
		//Otherwise, we'll add it in as one of the children
		add_child_node(member_list, member_node);

		//Now we will refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);

	//So long as we keep seeing semicolons
	} while (lookahead.tok == SEMICOLON);

	//Once we get here, what we know is that lookahead was not a semicolon. We know that it should
	//be a closing curly brace, so in the interest of better error messages, we'll do a little pre-check
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Construct members must be delimited by ;", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we get here we know that it's right, but we'll still allow the other rule to handle it
	push_back_token(fl, lookahead);

	//Give the member list back
	return member_list;
}


/**
 * A construct definer is the definition of a construct. We require all parts of the construct to be defined here.
 * We also allow the potential for aliasing as a different type right off of the bat here. Like all other rules, 
 * this function returns a pointer to the subtree that it creates
 *
 * REMEMBER: By the time we get here, we've already seen the define and construct keywords due to lookahead rules
 *
 * This rule also handles everything with identifiers to avoid excessive confusion
 *
 * BNF Rule: <construct-definer> ::= define construct <identifier> { <construct-member-list> } {as <identifer>}?;
 */
static generic_ast_node_t* construct_definer(FILE* fl){
	//For error printing
	char info[2000];
	//Freeze the line num
	u_int16_t current_line = parser_line_num;
	//Lookahead token for our uses
	Lexer_item lookahead;
	//The actual type name that we have
	char type_name[MAX_TYPE_NAME_LENGTH];
	
	//We already know that the type name will have enumerated in it
	strcpy(type_name, "construct ");

	//We are now required to see a valid identifier
	generic_ast_node_t* ident = identifier(fl);

	//Fail case
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Valid identifier required after construct keyword", parser_line_num);
		num_errors++;
		//Ident is already an error, just give it back
		return ident;
	}

	//Otherwise, we'll now add this identifier into the type name
	strcat(type_name, ((identifier_ast_node_t*)(ident->node))->identifier);	

	//Now we will reference against the symtab to see if this type name has ever been used before. We only need
	//to check against the type symtab because that is the only place where anything else could start with "enumerated"
	symtab_type_record_t* found = lookup_type(type_symtab, type_name);

	//This means that we are attempting to redefine a type
	if(found != NULL){
		sprintf(info, "Type with name \"%s\" was already defined. First defined here:", type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the type
		print_type_name(found);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we are required to see a curly brace
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unelaborated construct definition is not supported", parser_line_num);
		num_errors++;
		//Create and return the error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we'll push onto the stack for later matching
	push(grouping_stack, lookahead);

	//We are now required to see a valid construct member list
	generic_ast_node_t* mem_list = construct_member_list(fl);

	//Automatic fail case here
	if(mem_list->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid construct member list given in construct definition", parser_line_num);
		//It's already an error, so we'll just allow it to propogate
		return mem_list;
	}

	//Otherwise we got past the list, and now we need to see a closing curly
	lookahead = get_next_token(fl, &parser_line_num);

	//Bail out if this happens
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Closing curly brace required after member list", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Check for unamtched curlies
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces in construct definition", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//If we make it here, we've made it far enough to know what we need to build our type for this construct
	generic_type_t* construct_type = create_constructed_type(type_name, current_line);

	//Now we're going to have to walk the members of the member list, and add in their references to the type.
	//Doing this work now saves us a lot of steps later on for not much duplicated space
	//Start off with the first child
	generic_ast_node_t* cursor = mem_list->first_child;

	//As long as there are more children
	while(cursor != NULL){
		//Sanity check
		if(cursor->CLASS != AST_NODE_CLASS_CONSTRUCT_MEMBER){
			print_parse_message(PARSE_ERROR, "Fatal internal parse error. Found non-construct member in member list", parser_line_num);
			//Return an error and fail out
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Pick out the variable record
		symtab_variable_record_t* var = ((construct_member_ast_node_t*)(cursor->node))->member_var;

		//We'll now add this into the parameter list
		construct_type->construct_type->members[construct_type->construct_type->num_members] = var;
		//Increment the number of members
		(construct_type->construct_type->num_members)++;

		//Now that we've added it in, advance the cursor
		cursor = cursor->next_sibling;
	}

	//Once we're here, the construct type is fully defined. We can now add it into the symbol table
	insert_type(type_symtab, create_type_record(construct_type));
	
	//Now we'll actually create the node itself
	generic_ast_node_t* construct_definer_node = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_DEFINER);
	//Save the construct type in here
	((construct_definer_ast_node_t*)(construct_definer_node->node))->created_construct = construct_type;

	//We'll now add a reference to the construct 
	
	//This node's first child will be the identifier
	add_child_node(construct_definer_node, ident);

	//The next node is the member list
	add_child_node(construct_definer_node, mem_list);

	//Now we have one final thing to account for. The syntax allows for us to alias the type right here. This may
	//be preferable to doing it later, and is certainly more convenient. If we see a semicol right off the bat, we'll
	//know that we're not aliasing however
	lookahead = get_next_token(fl, &parser_line_num);

	//We're out of here, just return the node that we made
	if(lookahead.tok == SEMICOLON){
		return construct_definer_node;
	}
	
	//Otherwise, if this is correct, we should've seen the as keyword
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "Semicolon expected after construct definition", parser_line_num);
		num_errors++;
		//Make an error and get out of here
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now if we get here, we know that we are aliasing. We won't have a separate node for this, as all
	//we need to see now is a valid identifier. We'll add the identifier as a child of the overall node
	generic_ast_node_t* alias_ident = identifier(fl);

	//If it was invalid
	if(alias_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
		num_errors++;
		//It's already an error, so we'll just propogate it
		return alias_ident;
	}

	//Real quick, let's check to see if we have the semicol that we need now
	lookahead = get_next_token(fl, &parser_line_num);

	//Last chance for us to fail syntactically 
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after construct definition",  parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we have to check to make sure there are no name collisions
	//Grab this for convenience
	char* name = ((identifier_ast_node_t*)(alias_ident->node))->identifier;

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now if we make it all the way here, we know that we have a valid name that we can
	//alias with, so we'll first add the ident in to the tree
	add_child_node(construct_definer_node, alias_ident);

	//Now we'll make the actual record for the aliased type
	generic_type_t* aliased_type = create_aliased_type(name, construct_type, parser_line_num);

	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(aliased_type));

	//Give back the root level node
	return construct_definer_node;
}


/**
 * An enum member is simply an identifier. This rule performs all the needed checks to ensure
 * that it's not a duplicate of anything else that we've currently seen. Like all rules, this function
 * returns a reference to the root of the tree it created
 *
 * BNF Rule: <enum-member> ::= <identifier>
 */
static generic_ast_node_t* enum_member(FILE* fl){
	//For error printing
	char info[2000];

	//We really just need to see a valid identifier here
	generic_ast_node_t* ident = identifier(fl);

	//If it fails, we'll blow the whole thing up
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as enum member", parser_line_num);
		num_errors++;
		//It's already an error, so we'll just send it back
		return ident;
	}

	//Now if we make it here, we'll need to check and make sure that it isn't a duplicate of anything else
	//Grab this for convenience
	char* name = ((identifier_ast_node_t*)(ident->node))->identifier;

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we make it all the way down here, we know that we don't have any duplication
	//We can now make the record of the enum
	symtab_variable_record_t* enum_record = create_variable_record(name, STORAGE_CLASS_NORMAL);
	//Later down the line, we'll assign the type that this thing is
	
	//We can now add it into the symtab
	insert_variable(variable_symtab,  enum_record);

	//Finally, we'll construct the node that holds this item and send it out
	generic_ast_node_t* enum_member = ast_node_alloc(AST_NODE_CLASS_ENUM_MEMBER);
	//Store the record in this for ease of access/modification
	((enum_member_ast_node_t*)(enum_member->node))->member_var = enum_record;
	//Add the identifier as the child of this node
	add_child_node(enum_member, ident);

	//Finally we'll give the reference back
	return enum_member;
}


/**
 * An enumeration list guarantees that we have at least one enumerator. It also allows us to
 * chain enumerators using commas. Like all rules, this rule returns a reference to the root of 
 * the subtree that it creates
 *
 * BNF Rule: <enum-member-list> ::= <enum-member>{, <enum-member>}*
 */
static generic_ast_node_t* enum_member_list(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//We will first create the list node
	generic_ast_node_t* enum_list_node = ast_node_alloc(AST_NODE_CLASS_ENUM_MEMBER_LIST);

	//Now, we can see as many enumerators as we'd like here, each separated by a comma
	do{
		//First we need to see a valid enum member
		generic_ast_node_t* member = enum_member(fl);

		//If the member is bad, we bail right out
		if(member->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid member given in enum definition", parser_line_num);
			num_errors++;
			//It's already an error so we'll just send it up the chain
			return member;
		}

		//We can now add this in as a child of the enum list
		add_child_node(enum_list_node, member);

		//Finally once we make it here we'll refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);

	//Keep going as long as we see commas
	} while(lookahead.tok == COMMA);

	//Once we make it out here, we know that we didn't see a comma. We know that we really need to see an
	//R_CURLY when we get here, so if we didn't we can give a more helpful error message here
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Enum members must be separated by commas in defintion", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Otherwise if we end up here all went well. We'll let the caller do the final checking with the R_CURLY so 
	//we'll give it back
	push_back_token(fl, lookahead);

	//We'll now give back the list node itself
	return enum_list_node;
}


/**
 * An enumeration definition is where we see the actual definition of an enum. Like all other root nodes
 * in the language, it returns the root of the subtree that it created
 *
 * Important note: By the time we get here, we will have already consume the "define" and "enum" tokens
 *
 * BNF Rule: <enum-definer> ::= define enum <identifier> { <enum-member-list> } {as <identifier>}?;
 */
static generic_ast_node_t* enum_definer(FILE* fl){
	//For error printing
	char info[2000];
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//The actual name of the enum
	char name[MAX_TYPE_NAME_LENGTH];

	//We already know that it will have this in the name
	strcpy(name, "enum ");

	//We now need to see a valid identifier to round out the name
	generic_ast_node_t* ident = identifier(fl);

	//Fail case here
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid name given to enum definition", parser_line_num);
		num_errors++;
		//It's already an error so just return it
		return ident;
	}

	//Now if we get here we know that we found a valid ident, so we'll add it to the name
	strcat(name, ((identifier_ast_node_t*)(ident->node))->identifier);

	//Now we need to check that this name isn't already currently in use. We only need to check against the
	//type symtable, because nothing else could have enum in the name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//If we found something, that's an illegal redefintion
	if(found_type != NULL){
		sprintf(info, "Type \"%s\" has already been defined. First defined here:", name); 
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Print out the actual type too
		print_type_name(found_type);
		num_errors++;
		return 0;
	}

	//Now that we know we don't have a duplicate, we can now start looking for the enum list
	//We must first see an L_CURLY
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly expected before enumerator list", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push onto the stack for grouping
	push(grouping_stack, lookahead);
	
	//Now we must see a valid enum member list
	generic_ast_node_t* member_list = enum_member_list(fl);

	//If it failed, we bail out
	if(member_list->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid enumeration member list given in enum definition", current_line);
		//It's already an error so just send it back up
		return member_list;
	}

	//Now that we get down here the only thing left syntatically is to check for the closing curly
	lookahead = get_next_token(fl, &parser_line_num);

	//First chance to fail
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Closing curly brace expected after enum member list", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We must also see matching ones here
	if(pop(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected in enum defintion", parser_line_num);
		num_errors++;
		//Create and return the error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we know everything here has worked, we can finally create the enum type
	generic_type_t* enum_type = create_enumerated_type(name, current_line);

	//Now we will crawl through all of the types that we had and add their references into this enum type's list
	//This should in theory be an enum member node
	generic_ast_node_t* cursor = member_list->first_child;	

	//Go through while the cursor isn't null
	while(cursor != NULL){
		//Sanity check here, this should be of type enum member
		if(cursor->CLASS != AST_NODE_CLASS_ENUM_MEMBER){
			print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Found non-member node in member list for enum", parser_line_num);
			//Bail right out if this happens
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
		//Otherwise we're fine
		//We'll now extract the symtab record that this node holds onto
		symtab_variable_record_t* variable_rec = ((enum_member_ast_node_t*)(cursor->node))->member_var;

		//Associate the type here as well
		variable_rec->type = enum_type;

		//We will store this in the enum types records
		enum_type->enumerated_type->tokens[enum_type->enumerated_type->token_num] = variable_rec;
		//Increment the number of tokens by one
		(enum_type->enumerated_type->token_num)++;

		//Move the cursor up by one
		cursor = cursor->next_sibling;
	}

	//Now that this is all filled out, we can add this to the type symtab
	insert_type(type_symtab, create_type_record(enum_type));

	//Now once we get here, we will have added all of the sibling references in
	//We can now also create the overall definer node
	generic_ast_node_t* enum_def_node = ast_node_alloc(AST_NODE_CLASS_ENUM_DEFINER);
	//We will also now link this type to it
	((enum_definer_ast_node_t*)(enum_def_node->node))->created_enum = enum_type;

	//The enum def node first has the name ident as its child
	add_child_node(enum_def_node, ident);
	//The next child will be the enum definer list
	add_child_node(enum_def_node, member_list);

	//Now once we are here, we can optionally see an alias command. These alias commands are helpful and convenient
	//for redefining variables immediately upon declaration. They are prefaced by the "As" keyword
	//However, before we do that, we can first see if we have a semicol
	lookahead = get_next_token(fl, &parser_line_num);

	//This means that we're out, so just give back the root node
	if(lookahead.tok == SEMICOLON){
		return enum_def_node;
	}

	//Otherwise, it is a requirement that we see the as keyword, so if we don't we're in trouble
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "Semicolon expected after enum definition", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now if we get here, we know that we are aliasing. We won't have a separate node for this, as all
	//we need to see now is a valid identifier. We'll add the identifier as a child of the overall node
	generic_ast_node_t* alias_ident = identifier(fl);

	//If it was invalid
	if(alias_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
		num_errors++;
		//It's already an error, so we'll just propogate it
		return alias_ident;
	}

	//Real quick, let's check to see if we have the semicol that we need now
	lookahead = get_next_token(fl, &parser_line_num);

	//Last chance for us to fail syntactically 
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after enum definition",  parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we have to check to make sure there are no name collisions
	//Grab this for convenience
	char* alias_name = ((identifier_ast_node_t*)(alias_ident->node))->identifier;

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now if we make it all the way here, we know that we have a valid name that we can
	//alias with, so we'll first add the ident in to the tree
	add_child_node(enum_def_node, alias_ident);

	//Now we'll make the actual record for the aliased type
	generic_type_t* aliased_type = create_aliased_type(name, enum_type, parser_line_num);

	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(aliased_type));

	//Give back the root level node
	return enum_def_node;
}


/**
 * A type address specifier allows us to specify that a type is actually an address(&) or some kind of array of these types
 * There is no limit to how deep the array or address manipulation can go, so this rule is recursive.
 * In the interest of memory safety, ollie language requires array bounds for static arrays to be known at compile time
 *
 * BNF Rule: {type-address-specifier} ::= [<constant>]{type-address-specifier}
 * 										| &{type-address-specifier}
 * 										| epsilon
 */
static generic_ast_node_t* type_address_specifier(FILE* fl){
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
 * If we are using this rule, we are assuming that this type exists in the 
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
 * 						   | enumerated <type-identifier>
 * 						   | construct <type-identifier>
 * 						   | <type-identifier>
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
 * the array brackets or address indicator. Like all rules, the type specifier rule will
 * always return a reference to the root of the subtree it creates
 *
 * The type specifier itself is comprised of some type name and potential address specifiers
 * 
 * NOTE: This rule REQUIRES that the name actually be defined. This rule is not used for the 
 * definition of names itself
 *
 * BNF Rule: <type-specifier> ::= <type-name>{<type-address-specifier>}*
 */
static generic_ast_node_t* type_specifier(FILE* fl){
	//For error printing
	char info[2000];
	//Freeze the current line
	u_int8_t current_line = parser_line_num;
	//Lookahead var
	Lexer_item lookahead;

	//We'll first create and attach the type specifier node
	//At this point the node will be entirely blank
	generic_ast_node_t* type_spec_node = ast_node_alloc(AST_NODE_CLASS_TYPE_SPECIFIER);

	//Now we'll hand off the rule to the <type-name> function. The type name function will
	//return a record of the node that the type name has
	status = type_name(fl);

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
 * Like all other functions, this function returns a pointer to the 
 * root of the subtree it creates. Since there is no concrete node here,
 * this function is really just a multiplexing rule
 *
 * <declaration-partition>::= <function-definition>
 *                        	| <declaration>
 */
static generic_ast_node_t* declaration_partition(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	Lexer_item lookahead;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//We know that we have a function here
	//We consume the function token here, NOT in the function rule
	if(lookahead.tok == FUNC){
		//We'll just let the function definition rule handle this. If it fails, 
		//that will be caught above
		return function_definition(fl);

	//Otherwise it must be a declaration
	} else {
		//Put the token back
		push_back_token(fl, lookahead);

		//We'll simply return whatever the product of the declaration function is
		return declaration(fl);
	}
}


/**
 * Here is our entry point. Like all functions, this returns
 * a reference to the root of the subtree it creates
 *
 * BNF Rule: <program>::= {<declaration-partition>}*
 */
static generic_ast_node_t* program(FILE* fl){
	//Freeze the line number
	Lexer_item lookahead;

	//We first symbolically "see" the START token. The start token
	//is the lexer symbol that the top level node holds
	Lexer_item start;
	//We really only care about the tok here
	start.tok = START;
	
	//Create the ROOT of the tree
	ast_root = ast_node_alloc(AST_NODE_CLASS_PROG);

	//Assign the lexer item to it for completeness
	((prog_ast_node_t*)(ast_root->node))->lex = start;

	//We'll keep a temp reference to the pointer that we're working on
	generic_ast_node_t* current;
	
	//As long as we aren't done
	while((lookahead = get_next_token(fl, &parser_line_num)).tok != DONE){
		//Call declaration partition
		current = declaration_partition(fl);

		//It failed, we'll bail right out if this is the case
		if(current->CLASS == AST_NODE_CLASS_ERR_NODE){
			//Just return the erroneous node
			return current;
		}
		
		//Otherwise, we'll add this as a child of the root
		add_child_node(ast_root, current);
		//And then we'll keep right along
	}

	//All went well if we get here
	return 1;
}


/**
 * Entry point for our parser. Everything beyond this point will be called in a recursive-descent fashion through
 * static methods
*/
u_int8_t parse(FILE* fl){
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

	//Global entry/run point, will give us a tree with
	//the root being here
	generic_ast_node_t* prog = program(fl);

	//Timer end
	clock_t end = clock();
	//Crude time calculation
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

	//If we failed
	if(prog->CLASS == AST_NODE_CLASS_ERR_NODE){
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
	deallocate_ast(prog);

	return status;
}
