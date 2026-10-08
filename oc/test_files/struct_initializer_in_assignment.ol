/**
 * Author: Jack Robbins
 * Test our ability to use a struct initializer inside of an assignment
 */


define struct my_struct {
	x:i32;
	y:i64;
	z:f64;
	a:char;
};


pub fn main() -> i32 {
	declare a:mut struct my_struct;

	//Assignment should work
	a = {5, 6, 7, 8};
	
	OUNIT: [exit_status = 5]
	ret a:x;
}
