/**
* Author: Jack Robbins
* Testing how the compiler handles invalid address operation attemps
*/


pub fn main() -> i32 {
	let mut x:i32 = 3;

	//INVALID
	let mut y:i32** = &(&x);

	ret **y;
}
