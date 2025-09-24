/**
* Author: Jack Robbins
* This file will test the process of assigning/dealing with constants
*/

fn test() -> u32{
	ret 2;
}

pub fn main() -> i32 {
	let mut a:i32 = 3;
	let mut b:i16 = 3;

	let mut c:i16 = b + 6;

	let mut x:i16 = b + c;

	ret a + b + c;
}
