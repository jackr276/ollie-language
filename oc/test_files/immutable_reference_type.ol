/**
* Author: Jack Robbins
* Test the usage of an immutable reference type
*/

fn tester(x:i32&, y:i32&) -> i32 {
	ret x + y;
}

pub fn main() -> i32 {
	let mut x:i32 = 3;
	let mut y: i32 = 5;

	//Grab immutable references to them
	let x_ref:i32& = x;
	let y_ref:i32& = y;

	ret @tester(x_ref, y_ref);
	
}
