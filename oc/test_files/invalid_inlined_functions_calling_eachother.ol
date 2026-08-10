/**
 * Author: Jack Robbins
 * Test an invalid case where we have inlined functions calling eachother.
 */

declare inline fn call_b(i32) -> i32;

inline fn call_a(x:i32) -> i32 {
	ret @call_b(x);
}

inline fn call_b(x:i32) -> i32 {
	ret x + @call_a(7);
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @call_a(7);
}
