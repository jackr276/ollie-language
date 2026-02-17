/**
* Author: Jack Robbins
* Test the use of a parameter more than once
*/


$macro PARAM_TWICE(x)
	(x - 1) + (x - 2)
$endmacro


pub fn call_param_twice(y:i32, x:i32) -> i32 {
	let z:i32 = PARAM_TWICE((y - x));
	
	ret z;
}


pub fn main() -> i32 {
	ret 0;
}

