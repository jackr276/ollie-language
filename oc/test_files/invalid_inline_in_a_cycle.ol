/**
 * Author: Jack Robbins
 * Test the case where we have an invalid inline cycle
 */

declare pub fn bottom() -> i32;


pub fn top() -> i32 {
	ret @bottom();
}

//INVALID - this is in a cycle and is indirectly recursive
pub inline fn middle() -> i32 {
	ret @top();
}

pub fn bottom() -> i32 {
	ret @middle();
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
