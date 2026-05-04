/**
* Author: Jack Robbins
* Test our ability to take the memory address of a pass by copy struct param
*/

define struct passing_struct {
	x:i32;
	y:i64;
	z:char;
} as passing_struct;


pub fn pass_by_copy_struct(param_struct: passing_struct*) -> i32 {
	ret 5 + param_struct=>x + param_struct=>z;
}


/**
 * Take in the struct itself as a copy and grab it's pointer for use in
 * the call
 */
pub fn pass_by_copy_struct_pass_through(param_struct: passing_struct) -> i32 {
	ret @pass_by_copy_struct(&param_struct);
}

pub fn main() -> i32 {
	let my_struct:passing_struct = {1, 5, '\0'};

	//Should return 1 + 5 + '\0' which is 6 
	ret @pass_by_copy_struct_pass_through(my_struct);
}
