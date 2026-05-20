/**
* Author: Jack Robbins
* Test Ollie's handling for float negative 0. Float negaitve 0 technically
* is possible because IEEE 754 floating point notation has a sign bit. So
* if we have negative 0 like we do here, we should in theory see a 1 in the
* sign bit when we extract it
*/


pub fn main() -> i32 {
	let x:f32 = -0.0;

	OUNIT: [console = 1]
	ret (*(<u32*>(&x))) >> 31;
}
