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

#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../local_constant/local_constant.h"
#include "../utils/three_address_variable.h"
#include "../utils/three_address_constant.h"
#include "../utils/global_variable.h"
#include "../ast/ast.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/ollie_intermediary_representation.h"
#include "../utils/x86_assembly_instruction.h"
#include "../utils/x86_genpurpose_registers.h"
#include "../utils/x86_sse_registers.h"
#include "../utils/stack_management_structs.h"
#include "../utils/ollie_instruction.h"
#include <stdint.h>
#include <sys/types.h>

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
 * A helper function that converts a variable type to a string for debugging
 */
const char* variable_type_to_string(variable_type_t type);

/**
 * A debug function that converts an addressing mode to a human readable string
 */
const char* addressing_mode_to_string(memory_addressing_mode_t mode);

/**
 * A wrapper around our atomically increasing variable ID
 */
u_int32_t get_next_variable_id();

/**
 * Simply retrieves the current variable ID
 */
u_int32_t get_current_variable_id();

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
 * Is the given instruction a load operation or not?
 */
u_int8_t is_load_instruction(instruction_t* instruction);

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
 * Is this a pure constant assignment instruction?
 */
u_int8_t is_instruction_constant_assignment(instruction_t* instruction);

/**
 * Is this an unsigned multiplication instruction?
 */
u_int8_t is_unsigned_multplication_instruction(instruction_t* instruction);

/**
 * Is this constant value 0?
 */
u_int8_t is_constant_value_zero(three_addr_const_t* constant);

/**
 * Is this constant value positive?
 */
u_int8_t is_constant_value_positive(three_addr_const_t* constant);

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
 * Create and return a three address variable that represents the memory address of a stack passed
 * parameter. This is specifically used for copy assignment with pass by copy parameters like structs
 * and unions
 */
three_addr_var_t* emit_stack_param_memory_address_temp_var(generic_type_t* type, stack_region_t* region);

/**
 * A return by copy variable is a special kind of variable that represents the passing of %rdi
 * as the struct return address when we do a return by copy
 */
three_addr_var_t* emit_return_by_copy_var(generic_type_t* type);

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
 * Emit a constant directly based on whatever the type given is
 */
three_addr_const_t* emit_direct_integer_or_char_constant(int64_t value, generic_type_t* type);

/**
 * Emit a stack passed parameter offset constant
 */
three_addr_const_t* emit_stack_passed_parameter_offset_constant(stack_region_t* region, generic_type_t* type);

/**
 * Emit a push instruction. We only have one kind of pushing - quadwords - we don't
 * deal with getting granular when pushing
 */
instruction_t* emit_push_instruction(three_addr_var_t* pushee, u_int32_t line_number);

/**
 * Sometimes we just want to push a given register. We're able to do this
 * by directly emitting a push instruction with the register in it. This
 * saves us allocation overhead
 *
 * This rule is explicitly for GP registers
 */
instruction_t* emit_direct_gp_register_push_instruction(general_purpose_register_t reg);

/**
 * Sometimes we just want to pop a given register. We're able to do this
 * by directly emitting a pop instruction with the register in it. This
 * saves us allocation overhead
 *
 * This rule is explicitly for GP registers
 */
instruction_t* emit_direct_gp_register_pop_instruction(general_purpose_register_t reg);

/**
 * Emit a pop instruction. We only have one kind of popping - quadwords - we don't
 * deal with getting granular when popping 
 */
instruction_t* emit_pop_instruction(three_addr_var_t* popee, u_int32_t line_number);

/**
 * Emit a CLEAR instruction that is meant for the FP register to be zeroed out
 * This function only takes an assignee because that's all that we're clearing
 */
instruction_t* emit_floating_point_clear_instruction(three_addr_var_t* assignee, u_int32_t line_number);

/**
 * Emit a PXOR instruction that's already been instruction selected
 */
instruction_t* emit_pxor_instruction(three_addr_var_t* destination, three_addr_var_t* source, u_int32_t line_number);

/**
 * Emit a lea statement that has one operand and an offset
 */
instruction_t* emit_lea_offset_only(three_addr_var_t* assignee, three_addr_var_t* address_operand1, three_addr_const_t* address_offset, u_int32_t line_number);

/**
 * Emit a lea statement that has no multiplier, only operands
 */
instruction_t* emit_lea_operands_only(three_addr_var_t* assignee, three_addr_var_t* address_operand1, three_addr_var_t* address_operand2, u_int32_t line_number);

/**
 * Emit a lea statement that has a multiplier and operands
 */
instruction_t* emit_lea_multiplier_and_operands(three_addr_var_t* assignee, three_addr_var_t* address_operand1, three_addr_var_t* address_operand2, u_int64_t type_size, u_int32_t line_number);

/**
 * Emit a lea statement that is used for rip relative calculations
 */
instruction_t* emit_lea_rip_relative_constant(three_addr_var_t* assignee, three_addr_var_t* local_constant_variable, three_addr_var_t* instruction_pointer, u_int32_t line_number);

/**
 * Emit a lea with the index and scale only
 */
instruction_t* emit_lea_index_and_scale_only(three_addr_var_t* assignee, three_addr_var_t* address_offset, u_int64_t address_scale, u_int32_t line_number);

/**
 * Emit a statement using three vars and a binary operator
 * ALL statements are of the form: assignee <- op1 operator op2
*/
instruction_t* emit_binary_operation_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_var_t* op2, u_int32_t line_number); 

/**
 * Emit a statement using two vars and a constant
 */
instruction_t* emit_binary_operation_with_const_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* op2, u_int32_t line_number); 

/**
 * Emit a statement that only uses two vars of the form var1 <- var2
 */
instruction_t* emit_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, u_int32_t line_number);

/**
 * Emit a synthetic memory initialization statement. These will always be wiped away by the
 * optimizer
 */
instruction_t* emit_synthetic_memory_initialization(three_addr_var_t* memory_address_var, u_int32_t line_number);

/**
 * Emit a statement that only uses two vars of the form var1 <- var2
 *
 * This truncating assignment instruction is designed specifically and only
 * for the truncating cast AST node type
 */
instruction_t* emit_truncating_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, u_int32_t line_number);

/**
 * Emit a conditional movement statement. Unlike regular moves, we will also need to provide the conditional and conditional movement type for this
 */
instruction_t* emit_conditional_movement_statement(three_addr_var_t* assignee, three_addr_var_t* if_assignee, three_addr_var_t* else_assignee, three_addr_var_t* conditional, conditional_movement_type_t movement_type, u_int32_t line_number);

/**
 * Emit a conditional movement statement with the else being a constant. Unlike regular moves, we will also need to provide the conditional and conditional movement type for this
 */
instruction_t* emit_conditional_movement_with_const_statement(three_addr_var_t* assignee, three_addr_var_t* if_assignee, three_addr_const_t* else_assignee, three_addr_var_t* conditional, conditional_movement_type_t movement_type, u_int32_t line_number);

/**
 * Emit a memory copy statement from one memory region to another. This exists
 * purely as an OIR statement and is converted to moves later on down the road
 *
 * Note that both the assignee and the op1 should be memory address variables when
 * we do this
 */
instruction_t* emit_memory_copy_instruction(three_addr_var_t* assignee_memory_region, three_addr_var_t* source_memory_region, u_int64_t byte_amount_to_copy, u_int32_t line_number);

/**
 * Emit a store statement that only uses the base address
 */
instruction_t* emit_store_base_address_only(three_addr_var_t* base_address, three_addr_var_t* storee, generic_type_t* memory_write_type, u_int32_t line_number);

/**
 * Emit a store with a base address and an index value(variable offset). This maps
 * to an addressing mode of REGISTERS_ONLY
 */
instruction_t* emit_store_base_address_and_index(three_addr_var_t* base_address, three_addr_var_t* index, three_addr_var_t* storee, generic_type_t* memory_write_type, u_int32_t line_number);

/**
 * Emit a store with a base address and a constant offset value. This maps to 
 * an addressing mode of OFFSET_ONLY
 */
instruction_t* emit_store_base_address_and_constant_offset(three_addr_var_t* base_address, three_addr_const_t* offset, three_addr_var_t* storee, generic_type_t* memory_write_type, u_int32_t line_number);

/**
 * Emit a rip-relative store instruction. This maps to an addressing
 * mode of RIP_RELATIVE
 */
instruction_t* emit_store_rip_relative(three_addr_var_t* instruction_pointer, three_addr_var_t* rip_relative_variable, three_addr_var_t* storee, generic_type_t* memory_write_type, u_int32_t line_number);

/**
 * Emit a store with a base address and constant offset value. This specific
 * overload allows us to store a constant instead of a variable
 */
instruction_t* emit_constant_store_base_address_and_constant_offset(three_addr_var_t* base_address, three_addr_const_t* offset, three_addr_const_t* storee, generic_type_t* memory_write_type, u_int32_t line_number);

/**
 * Emit a load instruction that only uses the base address
 */
instruction_t* emit_load_base_address_only(three_addr_var_t* assignee, three_addr_var_t* base_address, generic_type_t* memory_read_type, u_int32_t line_number);

/**
 * Emit a load instruction with a base address and index value(variable offset). This maps
 * to an addressing mode of REGISTERS_ONLY
 */
instruction_t* emit_load_base_address_and_index(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_var_t* index, generic_type_t* memory_read_type, u_int32_t line_number);

/**
 * Emit a load with a base address and a constant offset. This maps to an
 * addressing mode of OFFSET_ONLY
 */
instruction_t* emit_load_base_address_and_constant_offset(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_const_t* constant_offset, generic_type_t* memory_read_type, u_int32_t line_number);

/**
 * Emit a rip-relative load. This maps to an addressing mode of RIP_RELATIVE
 */
instruction_t* emit_load_rip_relative(three_addr_var_t* assignee, three_addr_var_t* rip_relative_variable, three_addr_var_t* instruction_pointer, generic_type_t* memory_read_type, u_int32_t line_number);

/**
 * Emit a statement that is assigning a const to a var i.e. var1 <- const
 */
instruction_t* emit_assignment_with_const_instruction(three_addr_var_t* assignee, three_addr_const_t* constant, u_int32_t line_number);

/**
 * Emit a load statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_load_instruction(three_addr_var_t* assignee, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset, u_int32_t line_number);

/**
 * Emit a store statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_store_instruction(three_addr_var_t* source, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset, u_int32_t line_number);

/**
 * Emit a return statement. The return statement can optionally have a node that we're returning.
 * Returnee may or may not be null
 */
instruction_t* emit_ret_instruction(three_addr_var_t* returnee, u_int32_t line_number);

/**
 * Emit a raise statement. Unlike a ret statement we are guaranteed to have an op1 here
 * because we must always be raising an error
 */
instruction_t* emit_raise_instruction(three_addr_var_t* raised_error, u_int32_t line_number);

/**
 * Emit an increment instruction
 */
instruction_t* emit_inc_instruction(three_addr_var_t* incrementee, u_int32_t line_number);

/**
 * Emit a decrement instruction
 */
instruction_t* emit_dec_instruction(three_addr_var_t* decrementee, u_int32_t line_number);

/**
 * Emit a negation(negX) statement
 */
instruction_t* emit_neg_instruction(three_addr_var_t* negatee, u_int32_t line_number);

/**
 * Emit a bitwise not instruction
 */
instruction_t* emit_not_instruction(three_addr_var_t* var, u_int32_t line_number);

/**
 * Emit a left shift statement
 */
instruction_t* emit_left_shift_stmt_instruction(three_addr_var_t* assignee, three_addr_var_t* var, three_addr_var_t* shift_amount_var, three_addr_const_t* shift_amount_const, u_int32_t line_number);

/**
 * Emit a right shift statement
 */
instruction_t* emit_right_shift_instruction(three_addr_var_t* assignee, three_addr_var_t* var, three_addr_var_t* shift_amount, three_addr_const_t* shift_amount_const, u_int32_t line_number);

/**
 * Emit a logical not instruction
 */
instruction_t* emit_logical_not_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, u_int32_t line_number);

/**
 * Emit a jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_jmp_instruction(void* jumping_to_block);

/**
 * Emit a jump instruction directly
 */
instruction_t* emit_jump_instruction_directly(void* jumping_to_block, instruction_type_t jump_instruction_type);

/**
 * Emit a stack allocation statement
 */
instruction_t* emit_stack_allocation_ir_statement(three_addr_const_t* bytes_to_allocate);

/**
 * Emit a stack deallocation statement
 */
instruction_t* emit_stack_deallocation_ir_statement(three_addr_const_t* bytes_to_deallocate);

/**
 * Emit a branch statement
 */
instruction_t* emit_branch_statement(void* if_block, void* else_block, three_addr_var_t* relies_on, branch_type_t branch_type, u_int32_t line_number);

/**
 * Emit an indirect jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_indirect_jump_statement(void* jump_table, three_addr_var_t* index, u_int64_t multiplier);

/**
 * Emit a function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_function_call_instruction(symtab_function_record_t* func_record, three_addr_var_t* assigned_to, u_int32_t line_number);

/**
 * Emit an indirect function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_indirect_function_call_instruction(three_addr_var_t* function_pointer, three_addr_var_t* assigned_to, u_int32_t line_number);

/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
instruction_t* emit_asm_inline_instruction(generic_ast_node_t* asm_inline_node, u_int32_t line_number);

/**
 * Emit a "test if not 0 three address code statement"
 */
instruction_t* emit_test_if_not_zero_statement(three_addr_var_t* destination_variable, three_addr_var_t* being_tested, u_int32_t line_number);

/**
 * Emit a "test if not 0 three address code statement"
 */
instruction_t* emit_test_if_not_zero_for_const_statement(three_addr_var_t* destination_variable, three_addr_const_t* being_tested, u_int32_t line_number);

/**
 * Emit an idle statement
 */
instruction_t* emit_idle_instruction(u_int32_t line_number);

/**
 * Emit a fully formed global variable OIR address calculation with offset lea
 *
 * This will always produce instructions like: t8 <- global_var(%rip)
 */
instruction_t* emit_global_variable_address_calculation_with_offset_oir(three_addr_var_t* assignee, three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, three_addr_const_t* constant, u_int32_t line_number);

/**
 * Emit a fully formed global variable OIR address calculation lea
 *
 * This will always produce instructions like: t8 <- global_var(%rip)
 */
instruction_t* emit_global_variable_address_calculation_oir(three_addr_var_t* assignee, three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, u_int32_t line_number);

/**
 * Emit a fully formed global variable x86 address calculation lea
 */
instruction_t* emit_global_variable_address_calculation_x86(three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, generic_type_t* u64, u_int32_t line_number);

/**
 * Emit a starting offset calculation for the given elaborative param
 */
instruction_t* emit_elaborative_param_starting_offset_calculation(three_addr_var_t* result, three_addr_var_t* elaborative_param, u_int32_t line_number);

/**
 * Are two variables equal? A helper method for searching
 */
u_int8_t variables_equal(three_addr_var_t* a, three_addr_var_t* b);

/**
 * Are two variables equal regardless of their SSA status? This function should only ever be used
 * by the instruction selector, under very careful circumstances
 */
u_int8_t variables_equal_no_ssa(three_addr_var_t* a, three_addr_var_t* b);

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
 * Convert any given constant into an i64(signed long). This is mainly used for lea helpers
 * where we want to guarantee that everything is consistent
 */
three_addr_const_t* convert_constant_to_i64(three_addr_const_t* constant, generic_type_t* i64_type);

/**
 * Negate a three address constant
 */
three_addr_const_t* negate_three_address_consant(three_addr_const_t* constant);

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
 * Emit the right shift of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 >> constant2
 */
void right_shift_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * Emit the right shift of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 << constant2
 */
void left_shift_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

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

#endif /* INSTRUCTION_H */
