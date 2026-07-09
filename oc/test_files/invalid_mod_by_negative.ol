/**
* Author: Jack Robbins
* Test an invalid attempt to modulo by a negative number
*/


pub fn main() -> i32 {
	let x:i32 = 5;

	OUNIT: [fail_to_compile]
	ret x % -2;
}
