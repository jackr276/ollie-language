/**
* Author: Jack Robbins
* Test file for testing ret interference
*/

pub fn main() -> i32 {
	let a:mut i32 = 0;
	declare b:mut i32;
	declare c:mut i32;

	do {
		b = a + 3;
		c = a + b;

	} while(a < 9);

	ret c;
}
