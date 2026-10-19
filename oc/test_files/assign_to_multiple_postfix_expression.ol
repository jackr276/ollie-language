/**
 * Author: Jack Robbins
 * Simple helper that tests our ability to assign with multiple postfix expressions
 */


pub fn main() -> i32 {
	let x:mut i32[][] = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];

	x[2][1] = 111;

	OUNIT: [exit_status = 111]
	ret x[2][1];
}
