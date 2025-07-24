/**
* Author: Jack Robbins
* This program tests the functionality of returning a ternary operator
*/

fn main() -> i32 {
	let mut x:i32 := 2;
	let mut a:i32 := 3;
	let mut b:i32 := 3;
	
	//Return the ternary
	ret x == 0 ? a - 1 else b + 2;
}
