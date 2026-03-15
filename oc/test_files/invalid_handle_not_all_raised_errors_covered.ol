/**
* Author: Jack Robbins
* Test an invalid case where we do not cover the specific errors raised by the function
*/

define error divide_by_zero_error_t;

pub fn! divide_values(x:i32, y:i32) -> i32 raises(divide_by_zero_error_t) {
	//Basic error case here
	if(x == 0) {
		raise divide_by_zero_error_t;
	}

	ret x / y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 0;

	//Divide values specifically requires us to handle for the divide by zero error
	let result:i32 = @divide_values(x, y) handle(error => ret -1);

	ret result;
}

