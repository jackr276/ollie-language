/**
 * Author: Jack Robbins
 * Test a case where we have initializers inside of a handle statement
 */

define struct return_struct {
	x:mut i32;
	y:mut i32;
	z:mut i32[5];
	d:f64;
};

define error invalid_input_error;


//Define a dummy that will raise an error
inline fn! return_by_copy_with_errors(x:i32, y:i32) -> struct return_struct raises (invalid_input_error){
	if(x < 0 || y < 0) {
		raise invalid_input_error;
	}

	ret {x, y, [1, 2, 3, 4, 5], 4.44d};
}


pub fn main() -> i32 {
	declare ret_val1:mut struct return_struct;

	//See how this works, we should trigger a copy assignment
	ret_val1 = @return_by_copy_with_errors(-1, 1) handle (invalid_input_error => {0, 77, [0,0,0,0,0], 0},
														error => {0, 77, [0,0,0,0,0], 0});

	OUNIT: [exit_status = 77]
	ret ret_val1:y;
}
