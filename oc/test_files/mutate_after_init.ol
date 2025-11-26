/**
* Author: Jack Robbins
* Fail case: mutating after initialization
*/

pub fn main() -> i32 {
	let x:i32 = 3;

	//BAD - mutate after init
	x = 5;
	
	ret x;
}
