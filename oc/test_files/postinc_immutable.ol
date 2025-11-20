/**
* Author: Jack Robbins
* Attempt to postincrement an immutable variable
*/

pub fn main() -> i32 {
	let c:i32 = 3;

	//Should fail - it's immutable
	c++;

	ret c;
}
