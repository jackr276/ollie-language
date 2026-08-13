/**
 * Author: Jack Robbins
 * Most base case for an inlined function - literally just returns a constant so no variables
 * or anything
 */


inline fn my_fn() -> i32 {
	ret 5;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @my_fn();
}
