/**
 * Author: Jack Robbins
 * Test an inlined function that raises errors
 */

define error divide_by_zero_error;


inline fn! safe_modulo(x:i32, y:i32) -> i32 raises (divide_by_zero_error) {
	//Can't mod by zero
	if(y == 0){
		raise divide_by_zero_error;
	}

	ret x % y;
}


pub fn main() -> i32 {
	let result:mut i32 = 0;

	//Swallow all of the errors
	result += @safe_modulo(5, 0) handle (divide_by_zero_error => 0, error => 0);

	result += @safe_modulo(104, 25) handle (divide_by_zero_error => 0, error => 0);

	//Should return 0 + 4
	OUNIT: [exit_status = 4]
	ret result;
}
