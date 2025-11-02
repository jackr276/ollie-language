/**
 * Author: Jack Robbins
 *
 * This header file defines methods that are used in the production and interpretation of
 * three address code. Three address code is what the middle-level IR of the compiler
 * is, and occupies the basic blocks of the CFG. The end IR of Ollie will be an instruction. 
 * Everything begins its life as a three address code statement, and ends its life as an instruction
*/

#ifndef INSTRUCTION_H 
#define INSTRUCTION_H 

//For symtab linking
#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../ast/ast.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/ollie_intermediary_representation.h"
#include "../utils/x86_assembly_instruction.h"
#include "../utils/x86_genpurpose_registers.h"
#include "../utils/stack_management_structs.h"
#include <stdint.h>
#include <sys/types.h>

//An overall structure for an instruction. Instructions start their life
//as three address code statements, and eventually become assembly instructions
typedef struct instruction_t instruction_t;
//A struct that holds our three address variables
typedef struct three_addr_var_t three_addr_var_t;
//A struct that holds our three address constants
typedef struct three_addr_const_t three_addr_const_t;
//A struct that stores all of our live ranges
typedef struct live_range_t live_range_t;
//The definition of a global variable container
typedef struct global_variable_t global_variable_t;


/**
 * A global variable stores the variable itself
 * and it stores the value, if it has one
 */
struct global_variable_t{
	//The variable itself - stores the name
	symtab_variable_record_t* variable;
	//The value - if given - of the variable
	three_addr_const_t* value;
	//What is this variable's reference count?
	u_int16_t reference_count;
};


/**
 * What kind of jump do we want to select
 */
typedef enum{
	JUMP_CATEGORY_INVERSE,
	JUMP_CATEGORY_NORMAL,
} jump_category_t;


/**
 * Do we want to have a regular branch or an inverse
 * branch? An inverse branch will down the road set the conditional
 * to be the opposite of what is put in.
 *
 * So for example
 *
 * if(x == 3) then A else B
 *
 * Regular mode
 * cmp 3, x
 * je A <-----  if
 * jmp B <----- else
 *
 * Inverse mode
 * cmp 3, x
 * jne A <-----  if(inverted)
 * jmp B <----- else
 */
typedef enum {
	BRANCH_CATEGORY_NORMAL,
	BRANCH_CATEGORY_INVERSE
} branch_category_t;


/**
 * Define the kind of branch that we have in an ollie branch
 * command
 */
typedef enum{
	NO_BRANCH, //This is the default, and what we get when we have 0
	BRANCH_NE,
	BRANCH_E,
	BRANCH_NZ,
	BRANCH_Z,
	BRANCH_L, //Branch LT(SIGNED)
	BRANCH_G, //Branch GT(SIGNED)
	BRANCH_GE, //Branch GE(SIGNED)
	BRANCH_LE, //Branch LE(SIGNED)
	BRANCH_A, //Branch GT(UNSIGNED)
	BRANCH_AE, //Branch GE(UNSIGNED)
	BRANCH_B, //Branch LT(UNSIGNED)
	BRANCH_BE, //Branch LE(UNSIGNED)
} branch_type_t;


/**
 * What kind of memory addressing mode do we have?
 */
typedef enum{
	ADDRESS_CALCULATION_MODE_NONE = 0, //default is always none
	ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE, //(%rax) - only the deref depending on how much indirection
	ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST, //(%rax) - only the deref depending on how much indirection
	ADDRESS_CALCULATION_MODE_OFFSET_ONLY, // 4(%rax)
	ADDRESS_CALCULATION_MODE_REGISTERS_ONLY, // (%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET, // 4(%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE, // (%rax, %rcx, 8)
	ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE, // 4(%rax, %rcx, 8)
	ADDRESS_CALCULATION_MODE_GLOBAL_VAR //Super special case, we will use address_calc_reg2 as the offset like this: <val>(%rip)
} address_calculation_mode_t;


/**
 * For variable printing, where we're printing
 * matters. The user must specify if it's
 * block or inline mode
 */
typedef enum{
	PRINTING_VAR_INLINE,
	PRINTING_VAR_BLOCK_HEADER,
	PRINTING_VAR_IN_INSTRUCTION,
	PRINTING_LIVE_RANGES,
	PRINTING_REGISTERS, //Use the allocate registers for this
} variable_printing_mode_t;


/**
 * For our live ranges, we'll really only need the name and
 * the variables
 */
struct live_range_t{
	//Hold all the variables that it has
	dynamic_array_t* variables;
	//And we'll hold an adjacency list for interference
	dynamic_array_t* neighbors;
	//Hold the stack region as well
	stack_region_t* stack_region;
	//What function does this come from?
	symtab_function_record_t* function_defined_in;
	//Store the id of the live range
	u_int32_t live_range_id;
	//Store the assignment count - used for stack pointer fixing
	u_int16_t assignment_count;
	//The degree of this live range
	u_int16_t degree;
	//The interference graph index of it
	u_int16_t interference_graph_index;
	//Store the heuristic spill cost
	int16_t spill_cost;
	//What is the function parameter order here?
	u_int8_t function_parameter_order;
	//Does this carry a pre-colored value
	u_int8_t is_precolored;
	//What register is this live range in?
	general_purpose_register_t reg; 
	//The size of the variable in the live range
	variable_size_t size;
};


/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Link to the function record(NULL if not there)
	symtab_function_record_t* linked_function;
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//What live range is this variable associate with
	live_range_t* associated_live_range;
	//What is the stack region associated with this variable?
	stack_region_t* stack_region;
	//What is the ssa generation level?
	u_int32_t ssa_generation;
	//What's the temp var number
	u_int32_t temp_var_number;
	//What's the reference count of this variable.
	//This will be needed later on down the line in 
	//the instruction selector
	u_int32_t use_count;
	//What is the indirection level
	//Is this variable dereferenced in some way
	//(either loaded from or stored to)
	u_int8_t is_dereferenced;
	//Is this a temp variable?
	u_int8_t is_temporary;
	//Is this a stack pointer?
	u_int8_t is_stack_pointer;
	//What is the parameter number of this var? Used for parameter passing. If
	//it is 0, it's ignored
	u_int8_t parameter_number;
	//What is the size of this variable
	variable_size_t variable_size;
	//What register is this in?
	general_purpose_register_t variable_register;
};


/**
 * A three address constant always holds the value of the constant
 */
struct three_addr_const_t{
	//This is for string constants
	local_constant_t* local_constant;
	//We hold the type info
	generic_type_t* type;
	//Store the constant value in a union
	union {
		symtab_function_record_t* function_name;
		int64_t long_constant;
		double double_constant;
		float float_constant;
		int32_t integer_constant;
		char char_constant;
	} constant_value;
	//What kind of constant is it
	ollie_token_t const_type;
};


/**
 * A generic struct that encapsulates most of our instructions
 */
struct instruction_t{
	//Store inlined assembly in a string
	dynamic_string_t inlined_assembly;
	//What block holds this?
	void* block_contained_in;
	//We have 2 ways to jump. The if jump is our affirmative jump,
	//else is our alternative
	void* if_block;
	void* else_block;
	//For linked list properties -- the next statement
	instruction_t* next_statement;
	//For doubly linked list properties -- the previous statement
	instruction_t* previous_statement;
	//A three address code always has 2 operands and an assignee
	three_addr_var_t* op1;
	three_addr_var_t* op2;
	//For convenience: op1 can also be a const sometimes
	three_addr_const_t* op1_const;
	three_addr_var_t* assignee;
	//Now for the assembly operations, we have a source and destination
	three_addr_var_t* source_register;
	//We can have more than one source, usually for CMP instructions
	three_addr_var_t* source_register2;
	//If we're trying to move a constant in
	three_addr_const_t* source_immediate;
	//Our destination register/variable
	three_addr_var_t* destination_register;
	//Certain instructions like conversions, and divisions, have more
	//than one destination register
	three_addr_var_t* destination_register2;
	three_addr_const_t* offset;
	//The address calculation registers
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;
	//What stack region do we write to or read from
	stack_region_t* linked_stack_region;
	//The LEA addition
	u_int64_t lea_multiplicator;
	//The function called
	symtab_function_record_t* called_function;
	//The variable record
	symtab_variable_record_t* var_record;
	//What function are we currently in?
	symtab_function_record_t* function;
	//Generic parameter list - could be used for phi functions or function calls
	void* parameters;
	//What is the three address code type
	instruction_stmt_type_t statement_type;
	//What is the x86-64 instruction
	instruction_type_t instruction_type;
	//The actual operator, stored as a token for size requirements
	ollie_token_t op;
	//Is this a jump table? -- for use in switch statements
	u_int8_t is_jump_table;
	//Is this operation critical?
	u_int8_t mark;
	//Is this operation a "branch-ending" operation. This would encompass
	//things like if statement decisions and loop conditions
	u_int8_t is_branch_ending;
	//Cannot be coalesced
	u_int8_t cannot_be_combined;
	//Is this a converting move of some kind?
	u_int8_t is_converting_move;
	//Does this have a multiplicator
	u_int8_t has_multiplicator;
	//Is this a regular or inverse branch
	u_int8_t inverse_branch;
	//If it's a branch statment, then we'll use this
	branch_type_t branch_type;
	//What kind of address calculation mode do we have?
	address_calculation_mode_t calculation_mode;
	//The register that we're popping or pushing
	general_purpose_register_t push_or_pop_reg;
};

/**
 * Initialize the memory management system
 */
void initialize_varible_and_constant_system();

/**
 * A helper function for our atomically increasing temp id
 */
int32_t increment_and_get_temp_id();

/**
 * A helper function that will create a global variable for us
 */
global_variable_t* create_global_variable(symtab_variable_record_t* variable, three_addr_const_t* value);

/**
 * Insert an instruction in a block before the given instruction
 */
void insert_instruction_before_given(instruction_t* insertee, instruction_t* given);

/**
 * Insert an instruction in a block after the given instruction
 */
void insert_instruction_after_given(instruction_t* insertee, instruction_t* given);

/**
 * Declare that we are in a new function
 */
void set_new_function(symtab_function_record_t* func);

/**
 * Helper function to determine if an operator is a relational operator
 */
u_int8_t is_operator_relational_operator(ollie_token_t op);

/**
 * Helper function to determine if we have a store operation
 */
u_int8_t is_store_operation(instruction_t* statement);

/**
 * Helper function to determine if an operator is can be constant folded
 */
u_int8_t is_operation_valid_for_op1_assignment_folding(ollie_token_t op);

/**
 * Helper function to determine if an instruction is a binary operation
 */
u_int8_t is_instruction_binary_operation(instruction_t* instruction);

/**
 * Helper function to determine if an instruction is an assignment operation
 */
u_int8_t is_instruction_assignment_operation(instruction_t* instruction);

/**
 * Does a given operation overwrite it's source? Think add, subtract, etc
 */
u_int8_t is_destination_also_operand(instruction_t* instruction);

/**
 * Is this operation a pure copy? In other words, is it a move instruction
 * that moves one register to another?
 */
u_int8_t is_instruction_pure_copy(instruction_t* instruction);

/**
 * Is this an unsigned multiplication instruction?
 */
u_int8_t is_unsigned_multplication_instruction(instruction_t* instruction);

/**
 * Is this a division instruction?
 */

/**
 * Is this constant value 0?
 */
u_int8_t is_constant_value_zero(three_addr_const_t* constant);

/**
 * Is this constant value 1?
 */
u_int8_t is_constant_value_one(three_addr_const_t* constant);

/**
 * Is this constant a power of 2?
 */
u_int8_t is_constant_power_of_2(three_addr_const_t* constant);

/**
 * Create and return a temporary variable
*/
three_addr_var_t* emit_temp_var(generic_type_t* type);

/**
 * Create and return a temporary variable from a live range
*/
three_addr_var_t* emit_temp_var_from_live_range(live_range_t* range);

/**
 * Create and return a three address var from an existing variable. If 
 * we are assigning to a variable, that will create a new generation of variable.
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var);

/**
 * Emit a variable for an identifier node. This rule is designed to account for the fact that
 * some identifiers may have had their types casted / coerced, so we need to keep the actual
 * inferred type here
*/
three_addr_var_t* emit_var_from_identifier(symtab_variable_record_t* var, generic_type_t* inferred_type);

/**
 * Emit a variable copied from another variable
 */
three_addr_var_t* emit_var_copy(three_addr_var_t* var);

/**
 * Create and return a constant three address var
 */
three_addr_const_t* emit_constant(generic_ast_node_t* const_node);

/**
 * Emit a three_addr_const_t value that is a local constant(.LCx) reference. This helper function
 * will also help us add the string constant to the function as a local function reference
 */
three_addr_const_t* emit_string_constant(symtab_function_record_t* function, generic_ast_node_t* const_node);

/**
 * Emit a constant directly based on whatever the type given is
 */
three_addr_const_t* emit_direct_integer_or_char_constant(int64_t value, generic_type_t* type);

/**
 * Emit a push instruction. We only have one kind of pushing - quadwords - we don't
 * deal with getting granular when pushing
 */
instruction_t* emit_push_instruction(three_addr_var_t* pushee);

/**
 * Sometimes we just want to push a given register. We're able to do this
 * by directly emitting a push instruction with the register in it. This
 * saves us allocation overhead
 */
instruction_t* emit_direct_register_push_instruction(general_purpose_register_t reg);

/**
 * Emit a movzx(zero extend) instruction
 */
instruction_t* emit_movzx_instruction(three_addr_var_t* destination, three_addr_var_t* source);

/**
 * Emit a movsx(sign extend) instruction
 */
instruction_t* emit_movsx_instruction(three_addr_var_t* destination, three_addr_var_t* source);

/**
 * Emit a pop instruction. We only have one kind of popping - quadwords - we don't
 * deal with getting granular when popping 
 */
instruction_t* emit_pop_instruction(three_addr_var_t* popee);

/**
 * Sometimes we just want to pop a given register. We're able to do this
 * by directly emitting a pop instruction with the register in it. This
 * saves us allocation overhead
 */
instruction_t* emit_direct_register_pop_instruction(general_purpose_register_t reg);

/**
 * Emit a movX instruction
 *
 * This is used for when we need extra moves(after a division/modulus)
 */
instruction_t* emit_movX_instruction(three_addr_var_t* destination, three_addr_var_t* source);

/**
 * Emit a lea statement with no type size multiplier on it
 */
instruction_t* emit_lea_instruction_no_mulitplier(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2);

/**
 * Emit a statement that is in LEA form
 */
instruction_t* emit_lea_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size);

/**
 * Emit an indirect jump calculation that includes a block label in three address code form
 */
instruction_t* emit_indir_jump_address_calc_instruction(three_addr_var_t* assignee, void* jump_table, three_addr_var_t* op2, u_int64_t type_size);

/**
 * Emit a statement using three vars and a binary operator
 * ALL statements are of the form: assignee <- op1 operator op2
*/
instruction_t* emit_binary_operation_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_var_t* op2); 

/**
 * Emit a statement using two vars and a constant
 */
instruction_t* emit_binary_operation_with_const_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* op2); 

/**
 * Emit a conditional assignment statement
 */
instruction_t* emit_conditional_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t prior_operator, u_int8_t is_signed, u_int8_t inverse_assignment);

/**
 * Emit a statement that only uses two vars of the form var1 <- var2
 */
instruction_t* emit_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a store statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_store_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * an offset(op1), and the value we're storing(op2)
 */
instruction_t* emit_store_with_variable_offset_ir_code(three_addr_var_t* base_address, three_addr_var_t* offset, three_addr_var_t* storee);

/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * a constant offset(op1_const), and the value we're storing(op2)
 */
instruction_t* emit_store_with_constant_offset_ir_code(three_addr_var_t* base_address, three_addr_const_t* offset, three_addr_var_t* storee);

/**
 * Emit a load statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_load_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a load with offset ir code. We take in a base address(op1), 
 * an offset(op2), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_variable_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_var_t* offset);

/**
 * Emit a load with constant offset ir code. We take in a base address(op1), 
 * an offset(op1_const), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_constant_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_const_t* offset);

/**
 * Emit a statement that is assigning a const to a var i.e. var1 <- const
 */
instruction_t* emit_assignment_with_const_instruction(three_addr_var_t* assignee, three_addr_const_t* constant);

/**
 * Emit a memory access statement
 */
instruction_t* emit_memory_access_instruction(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a load statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_load_instruction(three_addr_var_t* assignee, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset);

/**
 * Emit a store statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_store_instruction(three_addr_var_t* source, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset);

/**
 * Emit a return statement. The return statement can optionally have a node that we're returning.
 * Returnee may or may not be null
 */
instruction_t* emit_ret_instruction(three_addr_var_t* returnee);

/**
 * Emit an increment instruction
 */
instruction_t* emit_inc_instruction(three_addr_var_t* incrementee);

/**
 * Emit a memory address assignment statement
 */
instruction_t* emit_memory_address_assignment(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a decrement instruction
 */
instruction_t* emit_dec_instruction(three_addr_var_t* decrementee);

/**
 * Emit a test statement 
 */
instruction_t* emit_test_statement(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2);

/**
 * Directly emit a test instruction - bypassing three_addr_code entirely - going right to a selected instruction
 */
instruction_t* emit_direct_test_instruction(three_addr_var_t* op1, three_addr_var_t* op2);

/**
 * Emit a negation(negX) statement
 */
instruction_t* emit_neg_instruction(three_addr_var_t* assignee, three_addr_var_t* negatee);

/**
 * Emit a bitwise not instruction
 */
instruction_t* emit_not_instruction(three_addr_var_t* var);


/**
 * Emit a left shift statement
 */
instruction_t* emit_left_shift_stmt_instruction(three_addr_var_t* assignee, three_addr_var_t* var, three_addr_var_t* shift_amount_var, three_addr_const_t* shift_amount_const);

/**
 * Emit a right shift statement
 */
instruction_t* emit_right_shift_instruction(three_addr_var_t* assignee, three_addr_var_t* var, three_addr_var_t* shift_amount, three_addr_const_t* shift_amount_const);

/**
 * Emit a logical not instruction
 */
instruction_t* emit_logical_not_instruction(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_jmp_instruction(void* jumping_to_block);

/**
 * Emit a jump instruction directly
 */
instruction_t* emit_jump_instruction_directly(void* jumping_to_block, instruction_type_t jump_instruction_type);

/**
 * Emit a branch statement
 */
instruction_t* emit_branch_statement(void* if_block, void* else_block, three_addr_var_t* relies_on, branch_type_t branch_type);

/**
 * Emit an indirect jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_indirect_jmp_instruction(three_addr_var_t* address);

/**
 * Emit a function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_function_call_instruction(symtab_function_record_t* func_record, three_addr_var_t* assigned_to);

/**
 * Emit an indirect function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_indirect_function_call_instruction(three_addr_var_t* function_pointer, three_addr_var_t* assigned_to);

/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
instruction_t* emit_asm_inline_instruction(generic_ast_node_t* asm_inline_node);

/**
 * Emit a phi function statement. Once emitted, these statements are for the exclusive use of the compiler
 */
instruction_t* emit_phi_function(symtab_variable_record_t* variable);

/**
 * Emit an idle statement
 */
instruction_t* emit_idle_instruction();

/**
 * Emit a setX instruction
 */
instruction_t* emit_setX_instruction(ollie_token_t op, three_addr_var_t* destination_register, u_int8_t is_signed);

/**
 * Emit a setne three address code statement
 */
instruction_t* emit_setne_code(three_addr_var_t* assignee);

/**
 * Emit a stack allocation statement
 */
instruction_t* emit_stack_allocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset);

/**
 * Emit a stack deallocation statement
 */
instruction_t* emit_stack_deallocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset);

/**
 * Are two variables equal? A helper method for searching
 */
u_int8_t variables_equal(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirection);

/**
 * Are two variables equal regardless of their SSA status? This function should only ever be used
 * by the instruction selector, under very careful circumstances
 */
u_int8_t variables_equal_no_ssa(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirection);

/**
 * Emit a complete, one-for-one copy of an instruction
 */
instruction_t* copy_instruction(instruction_t* copied);

/**
 * Emit the sum of two given constants. The result will overwrite the second constant given
 *
 * The result will be: constant2 = constant1 + constant2
 */
three_addr_const_t* add_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * select the appropriate branch statement given the circumstances, including operand and signedness
 */
branch_type_t select_appropriate_branch_statement(ollie_token_t op, branch_category_t branch_type, u_int8_t is_signed);

/**
 * Select the appropriate set type given the circumstances, including the operand and the signedness
 */
instruction_type_t select_appropriate_set_stmt(ollie_token_t op, u_int8_t is_signed);

/**
 * Is the given register caller saved?
 */
u_int8_t is_register_caller_saved(general_purpose_register_t reg);

/**
 * Is the given register callee saved?
 */
u_int8_t is_register_callee_saved(general_purpose_register_t reg);

/**
 * Pretty print a three address code statement
*/
void print_three_addr_code_stmt(FILE* fl, instruction_t* stmt);

/**
 * Print an instruction that has not yet been given registers
 */
void print_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode);

/**
 * Print a variable and everything about it. If the variable is in
 * "Block header" mode, we won't print out any dereferencing info
 */
void print_variable(FILE* fl, three_addr_var_t* variable, variable_printing_mode_t mode);

/**
 * Print all given global variables who's use count is not 0
 */
void print_all_global_variables(FILE* fl, dynamic_array_t* global_variables);

/**
 * Print a live range out
 */
void print_live_range(FILE* fl, live_range_t* live_range);

/**
 * Destroy a three address variable
*/
void three_addr_var_dealloc(three_addr_var_t* var);

/**
 * Destroy an entire three address code statement
*/
void instruction_dealloc(instruction_t* stmt);

/**
 * Destroy all variables
*/
void deallocate_all_vars();

/**
 * Destroy all constants
*/
void deallocate_all_consts();

#endif /* INSTRUCTION_H */
