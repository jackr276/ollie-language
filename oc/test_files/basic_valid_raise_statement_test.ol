/**
* Author: Jack Robbins
* Test a case where we actually have a valid raise statement
*/

define error divide_by_zero_error_t;

pub fn! divide_values(x:i32, y:i32) -> i32 raises(divide_by_zero_error_t) {
	//Basic error case here
	if(y == 0) {
		raise divide_by_zero_error_t;
	}

	ret x / y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 0;

	let result:i32 = @divide_values(x, y) handle(divide_by_zero_error_t => ret 0, error => ret -1);

	OUNIT: [exit_status = 0]
	ret result;
}

