/**
 * Author: Jack Robbins
 * Test an invalid case where we have indirect inline recursion
 */

declare fn B() -> i32;

//INVALID - this is indirectly recursive
inline fn A() -> i32 {
	ret @B();
}

fn B() -> i32 {
	ret @A();
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
