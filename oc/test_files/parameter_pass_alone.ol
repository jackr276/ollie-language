/**
* Author: Jack Robbins
* Testing a full parameter load alone
*/


fn parameter_pass(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let k:mut i32 = x + y + z;
	let cc:mut char = a + b + c;

	ret k + cc;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
