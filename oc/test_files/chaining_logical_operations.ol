/**
* Author: Jack Robbins
* Test the code generator's ability to handle chaining logical operations
*/

pub fn main() -> i32 {
	let x:i32 = 0x98;
	let z:i32 = 9999;
	let y:i32 = -11;
	let a:i32 = 125;
	let b:i32 = 33;

	ret (a > 3) || (b < 7) && (y + x > 3);
}
