/**
* Author: Jack Robbins
* Test in specific the instruction selector's ability to handle moving from a small bit constant(i8/i16) into
* 32 or 64 bit float. This should be done by first scaling the constant up to a 32 bit integer and going
* from there
*/

define error divide_by_zero_error_t;

pub fn! divide_values(x:f32, y:f32) -> f32 raises(divide_by_zero_error_t) {
	//Basic error case here
	if(x == 0) {
		raise divide_by_zero_error_t;
	}

	ret x / y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 0;

	let result:f32 = @divide_values(x, y) handle(
												divide_by_zero_error_t => <i8>0,
												error => <i8>5
												);
	ret result;
}
