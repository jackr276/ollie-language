/**
 * Author: Jack Robbins
 * Test a valid case where we are defining a predeclared inline function
 */

declare pub inline fn my_fn(i32) -> i32;


pub inline fn my_fn(x:i32) -> i32 {
	ret x << 2;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 20]
	ret @my_fn(5);
}
