/**
* Author: Jack Robbins
* Test an invalid nested assignment expression
*/

pub fn main() -> i32 {
	declare x:i32;

	//Should fail with(Expression is not assignable)
	(x = 3) = 5;

	ret x;
}
