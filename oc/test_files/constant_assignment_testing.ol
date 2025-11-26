/**
* Author: Jack Robbins
* This file will test the process of assigning/dealing with constants
*/

fn test() -> u32{
	ret 2;
}

pub fn main() -> i32 {
	let a:mut i32 = 3;
	let b:mut i16 = 3;

	let c:mut i16 = b + 6;

	let x:mut i16 = b + c;

	ret a + b + c;
}
