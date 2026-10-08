/**
 * Author: Jack Robbins
 * Test the case where we have an initializer that also has a memory copy statement in it. Ollie
 * does allow for this with structs exclusively
 */


define struct my_struct {
	x:i32[5];
	y:i64;
	z:char;
};


pub fn generate_struct(x:i32, y:i64, z:char) -> struct my_struct {
	ret {[x, x + 1, x + 2, x + 3, x + 4], y, z};
}


pub fn main() -> i32 {
	//These are all copy assignments
	let x:struct my_struct[3] = [@generate_struct(5, 6, 7), @generate_struct(6, 7, 8), @generate_struct(7, 8, 9)];

	OUNIT: [exit_status = 10]
	ret x[2]:x[3];
}
