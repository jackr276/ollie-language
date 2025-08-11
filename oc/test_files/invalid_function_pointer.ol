/**
* Author: Jack Robbins
* This file will test the use of function pointers
* when they are invalid
*/

/**
* Shares the same signature as subtract
*/
fn add(mut x:i32, y:i32) -> i32{
	ret x + y;
}

/**
* Shares the same signature as add
*/
fn subtract(mut x:i32, y:i32) -> i32{
	ret x - y;
}


fn main() -> i32 {
	//Define an arithmetic function pointer that takes in two i32's
	define fn(mut i32, i32) -> i32 as arithmetic_function;

	//Just a random variable
	let mut x:i32 := 3;

	//An attempt to call a variable that is not a function pointer. Should error out right here
	ret @x(1, 3);
}
