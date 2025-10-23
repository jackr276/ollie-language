/**
* Author: Jack Robbins
* Test file for testing ret interference
*/

pub fn main() -> i32 {
	let mut a:i32 = 0;
	declare mut b:i32;
	declare mut c:i32;

	do {
		b = a + 3;
		c = c + b;

	} while(a < 9);

	ret c;
}
