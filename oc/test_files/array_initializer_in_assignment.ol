/**
 * Author: Jack Robbins
 * Test our ability to use an array initializer inside of an assignment expression
 */


pub fn main() -> i32 {
	let x:mut i32[5] = [1, 2, 3, 4, 5];

	//This should work
	x = [6, 7, 8, 9, 10];

	OUNIT: [exit_status = 8]
	ret x[2];
}
