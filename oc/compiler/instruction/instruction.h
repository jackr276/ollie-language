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
 * This enumeration will tell the printer in the final
 * compiler steps what kind of global variable initializer
 * we have
 */
typedef enum {
	GLOBAL_VAR_INITIALIZER_NONE = 0, //Most common case, we have nothing
	GLOBAL_VAR_INITIALIZER_CONSTANT, //Just a singular constant
	GLOBAL_VAR_INITIALIZER_ARRAY //An array of constants
} global_variable_initializer_type_t;


/**
 * What type of variable is this? Variables
 * can be temporary, stack variables, or normal
 * vars
 */
typedef enum {
	VARIABLE_TYPE_TEMP,
	VARIABLE_TYPE_NON_TEMP,
	VARIABLE_TYPE_MEMORY_ADDRESS,
	VARIABLE_TYPE_LOCAL_CONSTANT,
	VARIABLE_TYPE_FUNCTION_ADDRESS, //For rip-relative function pointer loads
} variable_type_t;


/**
 * A global variable stores the variable itself
 * and it stores the value, if it has one
 */
struct global_variable_t{
	//The variable itself - stores the name
	symtab_variable_record_t* variable;

	//Could be a constant or an array of constants
	union {
		//The value - if given - of the variable
		three_addr_const_t* constant_value;
		//A dynamic array of constants, if we have that
		dynamic_array_t array_initializer_values;
	} initializer_value;

	//What is this variable's reference count?
	u_int16_t reference_count;
	//Store the initializer type
	global_variable_initializer_type_t initializer_type;
};


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
	dynamic_array_t variables;
	//And we'll hold an adjacency list for interference
	dynamic_array_t neighbors;
	//Hold the stack region as well
	stack_region_t* stack_region;
	//What function does this come from?
	symtab_function_record_t* function_defined_in;
	//Store the id of the live range
	u_int32_t live_range_id;
	//Store the heuristic spill cost
	u_int32_t spill_cost;
	//Store the assignment count - used for stack pointer fixing
	u_int32_t assignment_count;
	//Store the use count as well
	u_int32_t use_count;
	//The degree of this live range
	u_int16_t degree;
	//The interference graph index of it
	u_int16_t interference_graph_index;
	//What is the function parameter order here?
	u_int8_t function_parameter_order;
	//Does this carry a pre-colored value
	u_int8_t is_precolored;
	//Was this live range spilled?
	u_int8_t was_spilled;
	//What register is this live range in?
	general_purpose_register_t reg; 
};


/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//What live range is this variable associate with
	live_range_t* associated_live_range;

	union {
		//What is the stack region associated with this variable?
		stack_region_t* stack_region;
		//What is the local constant associate with this variable
		local_constant_t* local_constant;
		//Rip relative function name for loading function pointers
		symtab_function_record_t* rip_relative_function;

	} associated_memory_region;

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
	//Is this a stack pointer?
	u_int8_t is_stack_pointer;
	//What is the parameter number of this var? Used for parameter passing. If
	//it is 0, it's ignored
	u_int8_t parameter_number;
	//What is the size of this variable
	variable_size_t variable_size;
	//What register is this in?
	general_purpose_register_t variable_register;
	//What membership do we have if any
	variable_membership_t membership;
	//What type of variable is this
	variable_type_t variable_type;
};


/**
 * A three address constant always holds the value of the constant
 */
struct three_addr_const_t{
	//We hold the type info
	generic_type_t* type;

	//Store the constant value in a union
	union {
		int64_t signed_long_constant;
		u_int64_t unsigned_long_constant;
		double double_constant;
		float float_constant;
		int32_t signed_integer_constant;
		u_int32_t unsigned_integer_constant;
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
	//The offset constant if we have one
	three_addr_const_t* offset;
	//The RIP offset variable
	three_addr_var_t* rip_offset_variable;
	//The address calculation registers
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;
	//Generic parameter list - could be used for phi functions or function calls
	dynamic_array_t parameters;
	//What block holds this?
	void* block_contained_in;
	//We have 2 ways to jump. The if jump is our affirmative jump,
	//else is our alternative
	void* if_block;
	void* else_block;
	//What is the type of the memory that we are trying to access? This is done
	//to maintain separation from the base addresses and the memory that we're using
	generic_type_t* memory_read_write_type;
	//For lea multiplication
	u_int64_t lea_multiplier;
	//The function called
	symtab_function_record_t* called_function;
	//The variable record
	symtab_variable_record_t* var_record;
	//What function are we currently in?
	symtab_function_record_t* function;
	//What is the three address code type
	instruction_stmt_type_t statement_type;
	//What is the x86-64 instruction
	instruction_type_t instruction_type;
	//The actual operator, stored as a token for size requirements
	ollie_token_t op;
	//Is this operation critical?
	u_int8_t mark;
	//Is this operation a "branch-ending" operation. This would encompass
	//things like if statement decisions and loop conditions
	u_int8_t is_branch_ending;
	//Cannot be coalesced
	u_int8_t cannot_be_combined;
	//Is this a regular or inverse branch
	u_int8_t inverse_branch;
	//If it's a branch statment, then we'll use this
	branch_type_t branch_type;
	//What kind of address calculation mode do we have?
	address_calculation_mode_t calculation_mode;
	//What is the lea type(only used during the IR phase)
	oir_lea_type_t lea_statement_type;
	//Do we have a read, write, or no attempt to access memory(default)
	memory_access_type_t memory_access_type;
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
 * Helper function to determine if an operator is a relational operator
 */
u_int8_t is_operator_relational_operator(ollie_token_t op);

/**
 * Does operation generate truthful byte value
 *
 * This encompasses: >, >=, <, <=, !=, ==, ||, && because
 * they generate either a 0 or a 1
 */
u_int8_t does_operator_generate_truthful_byte_value(ollie_token_t op);

/**
 * Helper function to determine if we have a store operation
 */
u_int8_t is_store_operation(instruction_t* statement);

/**
 * Helper function to determine if we have a load operation
 */
u_int8_t is_load_operation(instruction_t* statement);

/**
 * Is the given instruction a load operation or not?
 */
u_int8_t is_load_instruction(instruction_t* instruction);

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
 * Is the destination actually assigned?
 */
u_int8_t is_move_instruction_destination_assigned(instruction_t* instruction);

/**
 * Is this operation a pure copy? In other words, is it a move instruction
 * that moves one register to another?
 */
u_int8_t is_instruction_pure_copy(instruction_t* instruction);

/**
 * Is this a pure constant assignment instruction?
 */
u_int8_t is_instruction_constant_assignment(instruction_t* instruction);

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
 * Is this constant a power of 2 that is lea compatible(1, 2, 4, 8)?
 */
u_int8_t is_constant_lea_compatible_power_of_2(three_addr_const_t* constant);

/**
 * Create and return a temporary variable
*/
three_addr_var_t* emit_temp_var(generic_type_t* type);

/**
 * Emit a local constant temp var
 */
three_addr_var_t* emit_local_constant_temp_var(local_constant_t* local_constant);

/**
 * Emit a function pointer temp var
 */
three_addr_var_t* emit_function_pointer_temp_var(symtab_function_record_t* function_record);

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
 * Create and return a three address var from an existing variable. These special
 * "memory address vars" will represent the memory address of the variable in question
*/
three_addr_var_t* emit_memory_address_temp_var(generic_type_t* type, stack_region_t* region);

/**
 * Create and return a three address var from an existing variable. These special
 * "memory address vars" will represent the memory address of the variable in question
*/
three_addr_var_t* emit_memory_address_var(symtab_variable_record_t* var);

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
 * Emit a three_addr_var_t value that is a local constant(.LCx) reference. This helper function
 * will also help us add the string constant to the function as a local function reference
 */
three_addr_var_t* emit_string_local_constant(symtab_function_record_t* function, generic_ast_node_t* const_node);

/**
 * Emit a function pointer variable. This variable is designed to be used exclusively with the rip-relative
 * addressing modes that are required for function pointers
 */
three_addr_var_t* emit_function_pointer_variable(symtab_function_record_t* function);

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
 * Emit a lea statement that has one operand and an offset
 */
instruction_t* emit_lea_offset_only(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_const_t* op1_const);

/**
 * Emit a lea statement that has no multiplier, only operands
 */
instruction_t* emit_lea_operands_only(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2);

/**
 * Emit a lea statement that has a multiplier and operands
 */
instruction_t* emit_lea_multiplier_and_operands(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size);

/**
 * Emit a lea statement that is used for rip relative calculations
 */
instruction_t* emit_lea_rip_relative_constant(three_addr_var_t* assignee, three_addr_var_t* local_constant_variable, three_addr_var_t* instruction_pointer);

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
instruction_t* emit_store_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1, generic_type_t* memory_write_type);

/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * an offset(op1), and the value we're storing(op2)
 */
instruction_t* emit_store_with_variable_offset_ir_code(three_addr_var_t* base_address, three_addr_var_t* offset, three_addr_var_t* storee, generic_type_t* memory_write_type);

/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * a constant offset(op1_const), and the value we're storing(op2)
 */
instruction_t* emit_store_with_constant_offset_ir_code(three_addr_var_t* base_address, three_addr_const_t* offset, three_addr_var_t* storee, generic_type_t* memory_write_type);

/**
 * Emit a load statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_load_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1, generic_type_t* memory_read_type);

/**
 * Emit a load with offset ir code. We take in a base address(op1), 
 * an offset(op2), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_variable_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_var_t* offset, generic_type_t* memory_read_type);

/**
 * Emit a load with constant offset ir code. We take in a base address(op1), 
 * an offset(op1_const), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_constant_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_const_t* offset, generic_type_t* memory_read_type);

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
instruction_t* emit_setX_instruction(ollie_token_t op, three_addr_var_t* destination_register, three_addr_var_t* relies_on, u_int8_t is_signed);

/**
 * Emit a setne three address code statement
 */
instruction_t* emit_setne_code(three_addr_var_t* assignee, three_addr_var_t* relies_on);

/**
 * Emit a fully formed global variable OIR address calculation with offset lea
 *
 * This will always produce instructions like: t8 <- global_var(%rip)
 */
instruction_t* emit_global_variable_address_calculation_with_offset_oir(three_addr_var_t* assignee, three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, three_addr_const_t* constant);

/**
 * Emit a fully formed global variable OIR address calculation lea
 *
 * This will always produce instructions like: t8 <- global_var(%rip)
 */
instruction_t* emit_global_variable_address_calculation_oir(three_addr_var_t* assignee, three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer);

/**
 * Emit a fully formed global variable x86 address calculation lea
 */
instruction_t* emit_global_variable_address_calculation_x86(three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, generic_type_t* u64);

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
 * Sum a constant by a raw int64_t value
 * 
 * NOTE: The result is always stored in the first one, and the first one will become 
 * a long constant. This is specifically designed for lea simplification/address computation
 */
three_addr_const_t* sum_constant_with_raw_int64_value(three_addr_const_t* constant, generic_type_t* i64_type, int64_t raw_constant);

/**
 * Multiply a constant by a raw int64_t value
 * 
 * NOTE: The result is always stored in the first one, and the first one will become 
 * a long constant. This is specifically designed for lea simplification
 */
three_addr_const_t* multiply_constant_by_raw_int64_value(three_addr_const_t* constant, generic_type_t* i64_type, int64_t raw_constant);

/**
 * Emit the product of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 * constant2
 */
void multiply_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * Emit the sum of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 + constant2
 */
void add_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * Emit the difference of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 - constant2
 */
void subtract_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * Logical or two constants. The result is always stored in constant1
 */
void logical_or_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * Logical and two constants. The result is always stored in constant1
 */
void logical_and_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

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
 * Get the estimated cycle count for a given instruction. This count
 * is of course estimated, we cannot know for sure
 */
u_int32_t get_estimated_cycle_count(instruction_t* instruction);

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
