/**
* Author: Jack Robbins
* Test an invalid case where we try to initialize a static variable to a non-constant value
*/

pub fn invalid_init(x:i32) -> i32 {
	//Invalid - x isn't comptime constant
	let static my_var:mut i32 = x;

	ret my_var++;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
