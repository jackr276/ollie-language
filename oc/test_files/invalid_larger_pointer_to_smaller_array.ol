/**
* Author: Jack Robbins
* Test the error handling when the user attempts to use array decay to
* get a larger pointer type to a smaller array type(say i64* to i32[])
*/

pub fn valid_case() -> i32 {
	declare arr:i32[50];

	//*SHOULD WORK*
	let x:i32* = arr;

	ret 0;
}

pub fn main() -> i32 {
	declare arr:i32[50];

	//Should not work
	let x:i64* = arr;

	ret 0;
}
