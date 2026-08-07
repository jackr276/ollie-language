/**
* Author: Jack Robbins
* Test file for testing using immutable variables in loops
*/

pub fn main() -> i32 {
	let a:mut i32 = 0;
	declare b:i32;
	declare c:i32;

	do {
		b = a + 3;
		c = a + b;

		a--;
	} while(a < 9);


	OUNIT: [fail_to_compile]
	ret c;
}
