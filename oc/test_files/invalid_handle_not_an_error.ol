/**
* Author: Jack Robbins
* Test an invalid case where we attempt to use a non-error inside of our handle statement
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

	let result:i32 = @divide_values(x, y) handle(
												divide_by_zero_error_t => ret -1,
												//This is not an error - cannot use it
												i32 => ret -2,
												error => ret -1
												);

	ret result;
}

