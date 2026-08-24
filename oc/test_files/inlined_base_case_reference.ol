/**
 * Author: Jack Robbins
 * This is the reference file for the inlined version used for development
 */


fn my_fn() -> i32 {
	ret 5;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @my_fn();
}
