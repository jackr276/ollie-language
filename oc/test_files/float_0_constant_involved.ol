/**
* Author: Jack Robbins
* Do a more involved test of floating point 0 constants to ensure that we're 
* not generating excessive PXOR interference. This has been an issue in the past
*/


pub fn float_arithmetic(x:f32, y:f32, z:i32) -> f32 {
	ret (x + y) * z;
}


pub fn main() -> i32 {
	let x:f32 = @float_arithmetic(2.22, 0.0, 8);

	let y:f32 = @float_arithmetic(0.0, -1.23, 4);

	ret x + y;
}
