/**
 * Author: Jack Robbins
 * Test our ability to use a struct initializer as an elaborative parameter. This will also test
 * our handling of memory address adjustment in function calls for initializers
 */

define struct internal_struct {
	y:i64;
	z:i64;
	aa:i32[5];
};

 
define struct my_struct {
 	x:i32;
	y:i64;
	z:i32;
	a:i32[5];
	inner:struct internal_struct;
 };


pub fn use_struct(input:params struct my_struct) -> i32 {
	ret input[0]:x + input[0]:z + input[0]:inner:aa[3];
}


pub fn main() -> i32 {	
	let x:struct internal_struct = {5, 6, [-5, -4, -3, -2, -1]};

	OUNIT: [exit_status = 6]
	ret @use_struct({5, 4, 3, [1, 2, 3, 4, 5], x});
}

