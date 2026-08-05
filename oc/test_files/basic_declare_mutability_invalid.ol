/**
* Author: Jack Robbins
* Basic mutability checking for simple values
*/


pub fn main() -> i32 {
	declare x:i32;

	//Totally valid
	x = 5;

	OUNIT: [exit_status = 5]
	ret x;
}
