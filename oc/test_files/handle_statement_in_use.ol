/**
* Author: Jack Robbins
* Test a case where we have a handle statement in use
*/

define error divide_by_zero_error_t;
define error invalid_modulo_error_t;

pub fn! divide_values(x:i32, y:i32) -> i32 raises(divide_by_zero_error_t, invalid_modulo_error_t) {
	//Basic error case here
	if(x == 0) {
		raise divide_by_zero_error_t;
	}

	if(y < 0 || x < 0){
		raise invalid_modulo_error_t;
	}

	ret x / y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 0;

	let result:i32 = @divide_values(x, y) handle(
												divide_by_zero_error_t => 5 + x,
												invalid_modulo_error_t => y - 5,
												error => 55
											);

	ret result + 5;
}

