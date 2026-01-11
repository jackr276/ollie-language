/**
* Author: Jack Robbins
* Test a more involved f32 add/subtract program for register 
* allocation
*/

fn float_add_tester(x:f32, y:f32, z:f32) -> f32 {
	let result:mut f32 = x + y;

	result -= z;
	
	ret result + 3;
}



pub fn main() -> i32 {
	ret @float_add_tester(2.33, 2.111, .33333);
}
