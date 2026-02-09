/**
* Author: Jack Robbins
* Test a very basic jump statement
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;

	x++;
	x--;
	
	//Unconditional jump
	jump end_label;

	//Unreachable
	let y:i32 = 3;

	#end_label:
	ret x;
}
