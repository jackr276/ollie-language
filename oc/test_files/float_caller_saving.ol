/**
* Author: Jack Robbins
* Test a more involved f32 add/subtract program for register 
* allocation
*/

SABOTAGED

fn manipulate_float_values(x:f32, y:f32) -> f32 {
	let result:mut f32 = x + y;

	let z:mut f32 = x - y;

	let result_helper:f32 = x - (y + z);

	result -= z;
	
	ret result + result_helper;
}


fn float_add_tester(x:f32, y:f32, z:f32) -> f32 {
	let result:mut f32 = x + y;

	let result_helper:f32 = x - (y + z);

	result -= @manipulate_float_values(y, z);
	
	ret result + result_helper;
}



pub fn main() -> i32 {
	ret @float_add_tester(2.33, 2.111, .33333);
}
