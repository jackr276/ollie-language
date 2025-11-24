/**
* Author: Jack Robbins
* Test an attempt to return a mutable void value
*/

//SHOULD FAIL
define struct my_struct {
	x:mut i32;
	y:mut i32;
	z:char;
} as bad_struct;

//It's not possible to return a mutable void value,
//this needs to error out
pub fn bad_func() -> mut void {
	ret 0;
}

pub fn main() -> i32 {
	declare str:mut bad_struct;
	
	ret 0;
}
