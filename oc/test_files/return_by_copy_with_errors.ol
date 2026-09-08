/**
 * Author: Jack Robbins
 * Test a case where we have a function that returns by copy and throws errors
 * to validate that we work properly
 */

define struct return_struct {
	x:mut i32;
	y:mut i32;
	z:mut i32[5];
	d:f64;
}

define error invalid_input_error;


//Define a dummy that will raise an error
fn! return_by_copy_with_errors(x:i32, y:i32) -> struct return_struct raises (invalid_input_error){
	if(x < 0 || y < 0) {
		raise invalid_input_error;
	}

	let ret_val:struct return_struct = {x, y, [1, 2, 3, 4, 5], 4.44d};
	ret ret_val;
}


pub fn main() -> i32 {
	
	let ret_val = 
	
}

