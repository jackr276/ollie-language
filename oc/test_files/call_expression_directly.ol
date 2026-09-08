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
	//Define an arithmetic function pointer that takes in two i32's
	define fn(mut i32, i32) -> i32 as arithmetic_function;

	let functions:mut arithmetic_function[] = [add, subtract, multiply];

	//Call into the function directly
	ret @functions[2](1, 3);
}
