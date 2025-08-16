/**
* Author: Jack Robbins
* This file will test the functionality of the ternary operator
*/

pub fn main() -> i32 {
	let mut x:i32 := 2;
	let mut a:i32 := 3;
	let mut b:i32 := 3;
	
	let mut test:i32 := x == 0 ? a else b;
	

	ret test;//c;
}
