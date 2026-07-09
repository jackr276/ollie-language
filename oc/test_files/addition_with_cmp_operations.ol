/**
* Author: Jack Robbins
* Test addition with comparison operations
*/

pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 6;
	let z:mut i32 = 7;
	
	//Chaining additions like this
	OUNIT: [exit_status = 1]
	ret (x > 7) + (y < z);
}
