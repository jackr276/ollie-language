/**
 * Author: Jack Robbins
 * Test an invalid case where we try to have a handles statement that initializes a string.
 * This is fundamentally invalid because there's nothing to initialize into for a function
 * that returns a string because they're return by reference
 */

define error invalid_input_error;

//Define a dummy that will raise an error
fn! return_array_with_errors(x:i32, y:i32) -> char[5] raises (invalid_input_error){
	if(x < 0) {
		raise invalid_input_error;
	}

	if(y < 0){
		raise error;
	}

	//Yeah this is dumb and bad just need to test
	let ret_val:char[5] = "bonk";
	ret ret_val;
}


pub fn main() -> i32 {
	//BAD!
	@return_array_with_errors(1, -1) handle (invalid_input_error => "abcd",
														  error => "1234");

	OUNIT: [fail_to_compile]
	ret 0;
}
