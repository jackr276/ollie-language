/**
* Author: Jack Robbins
* This test file aims to test an array of function pointers
*/

/**
* Shares the same signature as subtract
*/
fn add(x:mut i32, y:i32) -> i32{
	ret x + y;
}

/**
* Shares the same signature as add
*/
fn subtract(x:mut i32, y:i32) -> i32{
	ret x - y;
}

/**
* Shares the same signature as add
*/
fn multiply(x:mut i32, y:i32) -> i32{
	ret x * y;
}

pub fn main() -> i32{
	//Super ugly, but it should work
	declare functions:mut (fn(mut i32, i32) -> i32)[3];

	functions[0] = add;
	functions[1] = subtract;
	functions[2] = multiply;

	let x:mut fn(mut i32, i32) -> i32 = functions[2];

	ret @x(1, 3);
}


