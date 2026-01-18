/**
* Author: Jack Robbins
* Test the float caller saving functionality for indirect calls
*/


fn manipulate_float_values(x:f32, y:f32) -> f32 {
	let result:mut f32 = x + y;

	let z:mut f32 = x - y;

	let result_helper:f32 = x - (y + z);

	result -= z;
	
	ret result + result_helper;
}


fn float_add_tester(x:f32, y:f32, z:f32) -> f32 {
	//Float manipulator type
	define fn(f32, f32) -> f32 as float_manipulator;

	//Define it
	let func:float_manipulator = manipulate_float_values;

	let result:mut f32 = x + y;

	let result_helper:f32 = x - (y + z);

	result -= @func(y, z);

	++result;

	result -= @func(y, x);
	
	ret result + result_helper;
}


pub fn main() -> i32 {
	ret @float_add_tester(2.33, 2.111, .33333);
}
