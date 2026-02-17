/**
* Author: Jack Robbins
* Test two recursive parameters at once
*/

$macro INCREMENT(x)
	(x + 1)
$endmacro

$macro ADD(x, y)
	x + y
$endmacro


pub fn increment_tester(x:i32, y:i32) -> i32 {
	/**
	* Should expand to:
	*  (x + 1) + (y + 1)
	*/
	ret ADD(INCREMENT(x), INCREMENT(y));
}


pub fn main() -> i32 {
	ret 0;
}

