/**
 * Author: Jack Robbins
 * Test a fail case where we are trying to overload something as
 * inlined when the very first function we saw was not
 */


fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


//BAD!
inline fn add(x:f32, y:f32) -> f32 {
	ret x + y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
