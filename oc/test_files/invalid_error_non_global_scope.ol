/**
* Author: Jack Robbins
* Handle a case where a user tries to declare an error inside of a non-global scope
*/

pub fn main() -> i32 {
	//Should fail, non-global scope
	define error invalid_error;

	OUNIT: [fail_to_compile]
	ret 0;
}
