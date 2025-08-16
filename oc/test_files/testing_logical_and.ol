/**
* Author: Jack Robbins
* This will test the ability of the logical and functionality to handle
* something with a byte sized assignee
*/

fn tester() -> u8 {
	let mut x:u8 := 2;
	let mut y:u8 := 3;

	let mut z:u8 := x && y;

	ret z;
}

pub fn main() -> i32 {
	ret @tester();
}
