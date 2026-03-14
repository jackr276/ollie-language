/**
* Author: Jack Robbins
* Test the process of using division/modulus inside of a function call parameter
*/


pub fn div_params(x:i32, y:i32) -> i32 {
	ret x + y;
}

pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 3;
	let z:i32 = 7;

	ret @div_params(x / y, y % z);
}
