/**
* Author: Jack Robbins
* Testing the special conversion type from a 32-bit integer
* to a u64
*/


pub fn conversion_test() -> u64 {
	let x:mut u32 = 33;
	let y:mut u64 = 333;

	//This will convert implicitly
	ret y + x;
}


pub fn param_conversion_test(x:i32) -> u64 {
	let y:mut u64 = 333;

	//This will convert implicitly
	ret y + x;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
