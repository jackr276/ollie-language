/**
 * Author: Jack Robbins
 * Test that shows us using a struct initializer as a function parameter
 */

 define struct my_struct {
 	x:i32;
	y:i64;
	z:i32;
	a:i32[5];
 };


pub fn use_struct(input:struct my_struct) -> i32 {
	ret input:x + input:z;
}


pub fn main() -> i32 {	
	OUNIT: [exit_status = 8]
	ret @use_struct({5, 4, 3, [1, 2, 3, 4, 5]});
}
