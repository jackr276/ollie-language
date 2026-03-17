/**
* Author: Jack Robbins
* Test a valid handling of a void returning function
*/

define error divide_by_zero_error_t;
define error arithmetic_error_t;

pub fn! divide_values(x:mut i32*, y:i32) -> void raises(divide_by_zero_error_t, arithmetic_error_t) {
	//Basic error case here
	if(*x == 0) {
		raise divide_by_zero_error_t;
	}

	if(y == 0){
		raise arithmetic_error_t;
	}

	*x = *x / y;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:i32 = 0;

	@divide_values(&x, y) handle(
								divide_by_zero_error_t => ret -1,
								arithmetic_error_t => ignore,
								error => ret -1
								);
	ret x;
}
