/**
* Author: Jack Robbins
* Test the case where a user is attempting to take the address of an invalid function return
* value. Function return values must be either structs or unions to have their memory addresses
* taken
*
* This specific version test an indirect function call
*/


pub fn invalid_return_address(x:i32) -> i32 {
	ret x;
}


pub fn main() -> i32 {
	let func_ptr:fn(i32) -> i32 = invalid_return_address;

	//Should fail - this is not valid
	let x:i32* = &(@func_ptr(2));

	//Won't ever even get here
	ret *x;
}
