/**
* Author: Jack Robbins
* Test an attempt to define a union with a void member
*/

//SHOULD FAIL
define union my_union {
	x:i32;
	y:void;
	z:char;
} as bad_union;

pub fn main() -> i32 {
	declare un:mut bad_union;
	
	ret 0;
}
