/**
* Author: Jack Robbins
* In the effort for full test coverage, we are going to do a pass through case where we have
* a copy from an array access happening
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
pub fn pass_by_copy_struct_pass_through(param_struct_arr: passing_struct[4]) -> i32 {
	//Copy after array access
	ret @pass_by_copy_struct(param_struct_arr[2]);
}

pub fn main() -> i32 {
	let my_struct_arr:passing_struct[4] = [
										{1, 5, '\0'},
										{2, 6, '\0'},
										{3, 7, 'a'},
										{4, 8, '\0'}
										];

	//Should return 4 + 3 + 'a' which is 104 - coming from element 3 in the array
	ret @pass_by_copy_struct_pass_through(my_struct_arr);
}
