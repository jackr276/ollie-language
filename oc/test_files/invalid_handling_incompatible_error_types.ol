/**
* Author: Jack Robbins
* Test an invalid case where the type assignment is incompatible
*/

define error divide_by_zero_error_t;

pub fn! divide_values(x:i8, y:i8) -> i8 raises(divide_by_zero_error_t) {
	//Basic error case here
	if(x == 0) {
		raise divide_by_zero_error_t;
	}

	ret x / y;
}


pub fn main() -> i32 {
	//Can't assign a float to an i8 - this should fail
	let result:i8 = @divide_values(5, 0) handle(divide_by_zero_error_t => -4.333,  error => -3.232);

	ret result;
}

