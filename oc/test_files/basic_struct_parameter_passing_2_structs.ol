/**
* Author: Jack Robbins
* Super basic base case for passing a parameter through that happens to be a struct and
* will require a copy. 
*/

define struct passing_struct {
	x:i32;
	y:i64;
	z:char;
} as passing_struct;


pub fn pass_by_copy_struct(param_struct1: passing_struct, param_struct2: passing_struct) -> i32 {
	ret param_struct1:x + param_struct1:z + param_struct2:x + param_struct2:z;
}


pub fn main() -> i32 {
	let my_struct1:passing_struct = {1, 5, '\0'};
	let my_struct2:passing_struct = {3, 7, 'a'};

	//Should return 1 + '\0'(0) + 3 + 'a'(97)  = 101
	ret @pass_by_copy_struct(my_struct1, my_struct2);
}
