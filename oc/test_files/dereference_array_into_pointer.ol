/**
 * Author: Jack Robbins
 * Test an invalid case where we are dereferencing an array into a pointer
 */


pub fn main() -> i32 {
	let x:i32[5] = [1, 2, 3, 4, 5];
	let y:i32[5]* = &x;

	let z:i32* = *y;

	OUNIT: [exit_status = 4]
	ret z[3];
}
