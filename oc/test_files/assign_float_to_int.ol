/**
* Author: Jack Robbins
* Test the ability to assign a float constant to an integer with an implicit conversion
*/


//Should convert to an int constant
pub fn int_function() -> i32 {
	ret 3.33 + 8.97;
}

//Just to test passing an int in
pub fn int_param(x:i32) -> i32 {
	ret x;
}



pub fn main() -> i32 {
	//This should work, float to int is assignable
	declare y:i32;
	let x:i32 = 1.78;

	//Should force to int
	y = -2.22;

	//Test passing in
	@int_param(10.998);

	ret	x + y;
}
