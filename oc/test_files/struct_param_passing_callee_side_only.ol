/**
* Author: Jack Robbins
* This test file will only handle the callee-side of things to make sure
* that we are handling param passed structs appropriately
*/

define struct my_struct {
	c:char;
	x:i32[10];
	y:f32;
} as param_passed;


//Test to handle callee-side things
pub fn param_passed_struct(parameter_struct:struct my_struct) -> i32{
	ret parameter_struct:x[2] + parameter_struct:c + parameter_struct:y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
