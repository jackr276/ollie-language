/**
* Author: Jack Robbins
* Test the ability of the ollie parser to handle comma separated statements
*/

pub fn main() -> i32 {
	let x:mut i32 = 3, let y:mut i32 = 4, let z:mut i32 = 5;

	x += 1, y += 2, z += 3;

	ret x + y + z;
}
