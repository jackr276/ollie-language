/**
* Author: Jack Robbins
* Test an attempt to mod(still a divide) by a 0 constant. The parser should catch this and smack it down
*/

pub fn main() -> i32{
	let x:mut i32 = 3;

	//Should be caught by parser and hard fail
	let y:i32 = x % 0;

	ret y;
}
