/**
 * Author: Jack Robbins
 * Test our ability to postincrement array values
 */


pub fn main() -> i32 {
	let x:i32[] = [1, 2, 3, 4, 5];

	x[2]++;

	OUNIT: [exit_status = 4]
	ret x[2];
}
