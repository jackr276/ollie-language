/**
* Author: Jack Robbins
* As part of our language specification, we require that any elaborative params are always
* the very last parameter inside of the parameter list. Failure to have this should cause
* an error. This is what we are testing here
*/

pub fn invalid_not_last_elaborative(x:i32, y:params mut i32, z:i32) -> i32 {
	ret x + y + params[5];
}


//Dummy
pub fn main() -> i32 {	
	ret 0;
}
