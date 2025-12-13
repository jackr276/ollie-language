/**
* Author: Jack Robbins
* Test the most basic post-inc/dec case
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 5;

	//Super basic
	x--;
	y++;

	ret x + y;
}
