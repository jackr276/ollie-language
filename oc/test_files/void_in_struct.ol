/**
* Author: Jack Robbins
* Test an attempt to define a struct with a void member
*/

//SHOULD FAIL
define struct my_struct {
	x:mut i32;
	y:void;
	z:char;
} as bad_struct;

pub fn main() -> i32 {
	declare str:mut bad_struct;
	
	ret 0;
}
