/**
* Author: Jack Robbins
* Test the use case of a recursive macro parameter
*/

$macro REC_PARAM(x)
	(x + 2)
$endmacro

$macro TAKE_REC_PARAM(x, y)
	(x + y)
$endmacro


pub fn tester(x:i32, y:i32) -> i32 {
	//Recursive macro param
	ret TAKE_REC_PARAM(REC_PARAM(x), y);
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
