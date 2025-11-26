/**
* Author: Jack Robbins
* Testing how the compiler handles invalid address operation attemps
*/


pub fn main() -> i32 {
	let x:mut i32 = 3;

	//INVALID
	let y:mut i32** = &(&x);

	ret **y;
}
