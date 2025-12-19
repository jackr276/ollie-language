/**
* Author: Jack Robbins
* Test the compiler's ability to do equals chaining
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 3;
	let z:mut i32 = 3;

	ret x == y != z && x != 3;
}
