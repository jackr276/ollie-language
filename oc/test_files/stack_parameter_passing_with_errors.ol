/**
* Author: Jack Robbins
* Test stack parameter passing that happens more than once in an instance
*/

define error generic_error_t;

pub fn! involved_function_float(x:f32, y:f32, z:f32, aa:f32, bb:f32, cc:f32, dd:f32) -> f32 raises(generic_error_t) {
	if(x - y == 0){
		raise generic_error_t;
	}

	ret x + y + z + aa + bb + cc + dd;
}


pub fn! involved_function_int(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:i32) -> i32 raises(generic_error_t) {
	if(x - y == 0){
		raise generic_error_t;
	}

	ret x + y + z + aa + bb + cc + dd;
}


pub fn main() -> i32 {
	let x:f32 = @involved_function_float(32.3, .3, .4, .5, .6, .7, .8) handle(
																				generic_error_t => -1.0,
																				error => -1.0
																				);
	let y:i32 = @involved_function_int(32, 3, 4, 5, 6, 7, 8) handle(
																	generic_error_t => 2,
																	error => 2
																	);
	ret x + y;
}
