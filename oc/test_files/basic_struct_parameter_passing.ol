/**
* Author: Jack Robbins
* Super basic base case for passing a parameter through that happens to be a struct and
* will require a copy
*/

define struct passing_struct {
	x:i32;
	y:i64;
	z:char;
} as passing_struct;



pub fn pass_by_copy_struct(param_struct: passing_struct) -> i32 {
	ret 5 + param_struct:x + param_struct:z;
}


pub fn main() -> i32 {
	let my_struct:passing_struct = {1, 5, '\0'};

	//Should return 1 + 5 + '\0' which is 6
	ret @pass_by_copy_struct(my_struct);
}
