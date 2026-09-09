/**
 * Author: Jack Robbins
 * Test a case where we want to call a function pointer from a struct
 */


 define struct fn_ptr_struct {
 	x:i32;
	y:i32;
	ptr:fn(i32, i32) -> i32;
 };


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn build_struct(x:i32, y:i32) -> struct fn_ptr_struct {
	let ret_val:struct fn_ptr_struct = {x, y, add};

	ret ret_val;
}


pub fn main() -> i32 {
	let built_struct:struct fn_ptr_struct = @build_struct(13, 15);

	OUNIT: [exit_status = 28]
	ret @(built_struct:ptr)(built_struct:x, built_struct:y);
}
