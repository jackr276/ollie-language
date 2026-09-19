/**
 * Author: Jack Robbins
 * Test an invalid overload that does not differ enough using a mutable vs. immutable struct
 */


define struct my_struct {
	x:i32;
	y:i32;
	z:i64;
};


pub fn dummy(str:struct my_struct) -> i32 {
	ret str:x;
}


//BAD! mutability is not good enough
pub fn dummy(str:mut struct my_struct) -> i32 {
	ret str:x;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
