/**
 * Author: Jack Robbins
 * Test our ability to use a struct initializer as a parameter in an indirect call. This will also test
 * our handling of memory address adjustment in indirect function calls for initializers
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


pub fn use_struct(input:struct my_struct) -> i32 {
	ret input:x + input:z + input:inner:aa[4];
}


pub fn main() -> i32 {	
	let fn_ptr:fn(struct my_struct) -> i32 = use_struct;

	let x:struct internal_struct = {5, 6, [-5, -4, -3, -2, -1]};

	OUNIT: [exit_status = 7]
	ret @fn_ptr({5, 4, 3, [1, 2, 3, 4, 5], x});
}

