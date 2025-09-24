/**
* Author: Jack Robbins
* Testing the constant folding for division
*/

pub fn divide_by_negative() -> i32 {
	let mut x:i32 = 5;
	
	//Should *not* optimize
	let mut z:i32 = x / -4;
	
	ret z + x;
}

pub fn unsigned_shift() -> u32{
	let mut x:u32 = 3;
	let mut y:u32 = x - 1;

	//This should optimize into a logical left shift
	let mut z:u32 = y / 4;

	ret z + x;
}


pub fn main() -> i32{
	let mut x:i32 = 3;
	let mut y:i32 = x - 1;

	//This should optimize into an arithmetic right shift
	let mut z:i32 = y / 4;

	ret z + x;
}
