/**
 * Author: Jack Robbins
 * Test a case where we predeclare as inline but just declare as not inlined 
 */


declare inline fn my_fn(i32) -> i32;


//Invalid - can't have a difference here
fn my_fn(x:i32) -> i32 {
	ret 5;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @my_fn();
}
