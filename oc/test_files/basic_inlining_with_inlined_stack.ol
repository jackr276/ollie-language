/**
 * Author: Jack Robbins
 * Test a basic inlining case where we have an inlined function
 * that is going to use the stack
 */


inline fn uses_stack(x:i32) -> i32 {
	let arr:i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	ret arr[x];
}



pub fn main() -> i32 {
	let x:i32 = 5;

	OUNIT: [exit_status = 6]
	ret @uses_stack(x);
}
