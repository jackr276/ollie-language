/**
* Author: Jack Robbins
* Test a fail case where more parameters than needed are given
*/

$macro ADD(x, y)
	x + y
$endmacro


pub fn main() -> i32 {
	let x:i32 = 3;
	let y:i32 = 4;
	let z:i32 = 5;

	//Should fail, we have too many
	ret ADD(x, y, z);
}
