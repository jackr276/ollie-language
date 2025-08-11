/**
* Author: Jack Robbins
* This file will test the use of function pointers
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

	//This is the add function
	let mut x:arithmetic_function := add;

	//Should call add on 1 and 3
	ret @x(1, 3);
}
