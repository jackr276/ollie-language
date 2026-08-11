/**
 * Author: Jack Robbins
 * Test a case where we predeclare as pub inline but just declare as inline
 */


declare pub inline fn my_fn(i32) -> i32;


inline fn my_fn(x:i32) -> i32 {
	ret 5;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @my_fn();
}
