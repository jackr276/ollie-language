/**
 * Author: Jack Robbins
 * Test the use of a struct initializer in a return by copy statement in an 
 * inlined function
 */


define struct my_struct {
	x:i32;
	y:i64;
	z:f64;
	a:char;
};


pub inline fn build_struct(x:i32, y:i64, z:i64, a:char) -> struct my_struct {
	ret {x, y, z, a};
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @build_struct(5, 4, 3, 2):x;
}

