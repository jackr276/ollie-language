/**
* Author: Jack Robbins
* Attempt to assign to immutable array
*/

pub fn main() -> i32 {
	declare arr:i32[33];

	//Should fail mutability checks
	arr[2] = 3;

	ret 0;
}
