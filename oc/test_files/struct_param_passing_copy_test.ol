/**
* Author: Jack Robbins
* This test file again only deals with the callee side. We need to see if
* we're able to perform a local copy internally to a locally allocated struct
*/

define struct my_struct {
	c:char;
	x:i32[10];
	y:f32;
} as param_passed;


//Test to handle callee-side things
pub fn param_passed_struct(parameter_struct:struct my_struct) -> i32{
	/**
	* Copy from a parameter struct to a local struct
	*/
	let local_struct:param_passed = parameter_struct;


	ret local_struct:x[2] + local_struct:c + local_struct:y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
