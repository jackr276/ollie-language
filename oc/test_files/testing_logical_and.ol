/**
* Author: Jack Robbins
* This will test the ability of the logical and functionality to handle
* something with a byte sized assignee
*/

fn tester() -> u8 {
	let x:mut u8 = 2;
	let y:mut u8 = 3;

	let z:mut u8 = x && y;

	ret z;
}

pub fn main() -> i32 {
	ret @tester();
}
