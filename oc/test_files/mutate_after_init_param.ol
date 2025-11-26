/**
* Author: Jack Robbins
* Fail case: mutating after initialization
*/

pub fn tester(x:i32) -> i32 {
	//BAD - cannot mutate
	x++;

	ret 0;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;

	@tester(x);
	
	ret x;
}
