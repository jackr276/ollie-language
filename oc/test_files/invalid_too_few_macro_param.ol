/**
* Author: Jack Robbins
* Test a fail case where less parameters than needed are given
*/

$macro ADD(x, y)
	x + y
$endmacro


pub fn main() -> i32 {
	let x:i32 = 3;

	//Should fail, we have too few
	ret ADD(x);
}
