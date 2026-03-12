/**
* Author: Jack Robbins
* Test the case where we have a raises statement that also uses a defer statement.
* We need to validate that the raises statements does not have the defers inserted
* before it
*/

define error divide_by_zero_error;

pub fn! divide_numbers(x:mut i32, y:i32) -> i32 raises (divide_by_zero_error) {
	let result:i32 = 0;

	defer {
		x -= 1;
	}

	if(y < 0) {
		raise divide_by_zero_error;
	}

	ret x / y;
}
