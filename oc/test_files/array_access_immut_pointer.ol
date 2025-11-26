/**
* Author: Jack Robbins
* Attempt to assign to immutable array
*/

pub fn tester(x:i32*) -> void{
	//Should fail
	x[3] = 3;
}

pub fn main() -> i32 {
	declare arr:i32[33];

	@tester(arr);

	ret 0;
}
