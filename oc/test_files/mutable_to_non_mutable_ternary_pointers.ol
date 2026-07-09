/**
* Author: Jack Robbins
* Test a case where we have a mix of mutable and immutable pointers that we are assigning
* over to a non-mutable field. This test is OUNIT compatible
*/

pub fn tester(param:i32) -> i32* {
	let x:mut i32 = 2;
	let y:mut i32 = 3;
	
	let x_ptr:i32* = &x;
	let y_ptr:mut i32* = &y;

	//Should work just fine becasuse the result is immutable
	ret param > 5 ? y_ptr else x_ptr;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 2]
	ret *@tester(5);
}
