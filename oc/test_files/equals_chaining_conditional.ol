/**
* Author: Jack Robbins
* Test the compiler's ability to do equals chaining inside
* of conditionals
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 3;
	let z:mut i32 = 3;

	while(y == z != x){
		x--;
		y++;
		z = x || y;
	}


	ret z;
}
