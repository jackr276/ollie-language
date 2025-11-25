/**
* Author: Jack Robbins
* Testing the constant folding for multiplication 
*/

pub fn multiply_by_negative() -> i32 {
	let x:mut i32 = 5;
	
	//Should *not* optimize
	let z:mut i32 = x * -4;
	
	ret z + x;
}

pub fn unsigned_shift() -> u32{
	let x:mut u32 = 3;
	let y:mut u32 = x - 1;

	//This should optimize into a logical right shift
	let z:mut u32 = y * 4;

	ret z + x;
}


pub fn main() -> i32{
	let x:mut i32 = 3;
	let y:mut i32 = x - 1;

	//This should optimize into an arithmetic right shift
	let z:mut i32 = y * 4;

	ret z + x;
}
