/**
 * Author: Jack Robbins
 * Test a case where we have one function that is calling multiple inlined functions.
 * This will test our inlining abilities where we have to clone more than once
 */

inline fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


inline fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 7;

	OUNIT: [exit_status = 14]
	ret @add(5, 7) + @sub(7, 5);
}
