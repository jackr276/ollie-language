/**
* Author: Jack Robbins
* Test our ability to modify the parameter and have that modification reflect
*/

define struct passing_struct {
	x:mut i32;
	y:i64;
	z:char;
} as passing_struct;


pub fn pass_by_copy_struct(param_struct: passing_struct) -> i32 {
	ret 5 + param_struct:x + param_struct:z;
}


pub fn pass_by_copy_struct_pass_through_mod(param_struct: mut passing_struct) -> i32 {
	param_struct:x = 8;

	ret @pass_by_copy_struct(param_struct);
}


pub fn main() -> i32 {
	let my_struct:mut passing_struct = {1, 5, '\0'};

	//Should return 8 + 5 + '\0' which is 13
	ret @pass_by_copy_struct_pass_through_mod(my_struct);
}
