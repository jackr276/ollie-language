/**
* Author: Jack Robbins
* This file will test the functionality of the ternary operator
*/

pub fn main() -> i32 {
	let mut x:i32 = 2;
	let mut a:i32 = 3;
	let mut b:i32 = 3;

	let mut test:i32 = 0;
	
	//Ternary inside of the for loop
	for(let mut i:i32 = a == 3 ? x else 1; i < 32; i++) {
		test++;
	}

	ret test;
}
