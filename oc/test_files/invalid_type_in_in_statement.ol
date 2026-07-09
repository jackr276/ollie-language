/**
* Author: Jack Robbins
* Test an attempt to use an incompatible type inside of an in statement
*/


pub fn invalid_in_statement(x:i8) -> i32 {
	//Should fail due to the double
	ret x in (1, 2, 3, 4.5232d, 8);
}


//Dummy
pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
