/**
* Author: Jack Robbins
* In the effort for full test coverage, we are going to do a pass through case where we have
* a dereference copy happening
*/

define struct passing_struct {
	x:i32;
	y:i64;
	z:char;
} as passing_struct;


pub fn pass_by_copy_struct(param_struct: passing_struct) -> i32 {
	ret 5 + param_struct:x + param_struct:z;
}


/**
* Takes in a pointer to struct that we will need to dereference to get 
* the copy to happen
*/
pub fn pass_by_copy_struct_pass_through(param_struct: passing_struct*) -> i32 {
	//Copy after deref
	ret @pass_by_copy_struct(*param_struct);
}

pub fn main() -> i32 {
	let my_struct:passing_struct = {1, 5, '\0'};

	//Should return 1 + 5 + '\0' which is 6 - passed by reference
	ret @pass_by_copy_struct_pass_through(&my_struct);
}
