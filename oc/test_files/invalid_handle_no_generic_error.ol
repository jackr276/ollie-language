/**
* Author: Jack Robbins
* Test an invalid case where we are missing the generic error catch inside of our statement. As a reminder
* every errorable function must have a catch-all for the generic error case
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

	//Missing the generic error - this is wrong
	let result:i32 = @divide_values(x, y) handle(
												divide_by_zero_error_t => ret -1
												);

	ret result;
}
