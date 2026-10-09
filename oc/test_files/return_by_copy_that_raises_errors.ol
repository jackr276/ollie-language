/**
 * Author: Jack Robbins
 * Verify that the error route on a return-by-copy function that raises errors works properly
 */


define struct my_struct{
	x:i32[5];
	y:i64;
	z:f64;
};


define error invalid_input_error_t;


pub fn! return_by_copy_with_error(x:i32, y:i64) -> struct my_struct raises (invalid_input_error_t) {
	if(x < 0) {
		raise invalid_input_error_t;
	}

	let returned_struct:struct my_struct = {[x, x + 1, x + 2, x + 3, x + 4], y, 0};
	ret returned_struct;
}


pub fn main() -> i32 {
	//This will be our substitute
	let dummy_struct:struct my_struct = {[1, 2, 3, 4, 5], 8, 7};

	let result1:struct my_struct = @return_by_copy_with_error(-1, 8) handle(
																			error => dummy_struct,
																			invalid_input_error_t => dummy_struct
																			);

	let result2:struct my_struct = @return_by_copy_with_error(5, 8) handle(
																			error => dummy_struct,
																			invalid_input_error_t => dummy_struct
																			);

	//Should return 5 + 9 = 14
	OUNIT: [exit_status = 14]
	ret result1:x[4] + result2:x[4];
}
