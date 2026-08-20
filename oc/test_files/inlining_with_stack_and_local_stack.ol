/**
 * Author: Jack Robbins
 * Test inlining with a stack where the callee also has a stack
 */


inline fn uses_stack(x:i32) -> i32 {
	let arr:i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	ret arr[x];
}


inline fn has_stack(x:i32) -> i32 {
	let arr2:i32[] = [5, 6, 7];

	if(x > 2) {
		ret 0;
	} else {
		ret @uses_stack(arr2[x]);
	}
}


pub fn main() -> i32 {
	let x:i32 = 1;

	OUNIT: [exit_status = 7]
	ret @has_stack(x);
}
