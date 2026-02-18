/**
* Author: Jack Robbins
* Test the fail case/error message printing when too many parameters are given
*/

$macro ADD(x, y)
	(x + y)
$endmacro


pub fn main() -> i32 >{
	let x:i32 = 5;
	let y:i32 = 7;
	let z:i32 = 11;
	let aa:i32 = 17;

	//Far too many, let's see the error message it generates
	ret ADD(x, y, z, aa);
}
