/**
 * Author: Jack Robbins
 * Test an inlined function that handles raised errors
 */

define error divide_by_zero_error;


inline fn! safe_divide(x:i32, y:i32) -> i32 raises (divide_by_zero_error) {
	//Can't divide by zero
	if(y == 0){
		raise divide_by_zero_error;
	}

	ret x / y;
}

inline fn divide_numbers(x:i32, y:i32, z:i32) -> i32 {
	let result:mut i32 = 0;

	//Swallow all of the errors
	result += @safe_divide(x, z) handle (divide_by_zero_error => 0, error => 0);
	result += @safe_divide(x, y) handle (divide_by_zero_error => 0, error => 0);
	

	//Should return 0 + 4
	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 4]
	ret @divide_numbers(600, 150, 0);
}
