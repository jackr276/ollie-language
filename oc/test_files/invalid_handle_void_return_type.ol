/**
* Author: Jack Robbins
* Test an invalid case where the type assignment is incompatible
*/

define error divide_by_zero_error_t;

pub fn! divide_values(x:mut i8, y:i8) -> void raises(divide_by_zero_error_t) {
	//Basic error case here
	if(x == 0) {
		raise divide_by_zero_error_t;
	}

	x = x / y;

	ret;
}


pub fn main() -> i32 {
	//We have a 
	let result:i32 = @divide_values(5, 0) handle(divide_by_zero_error_t => 5,  error => 5);

	ret result;
}

