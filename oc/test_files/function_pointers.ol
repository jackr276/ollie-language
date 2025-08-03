/**
* Author: Jack Robbins
* This file will test the use of function pointers
*/

/**
* Shares the same signature as subtract
*/
fn add(x:i32, y:i32) -> i32{
	ret x + y;
}

/**
* Shares the same signature as add
*/
fn subtract(x:i32, y:i32) -> i32{
	ret x - y;
}


fn main() -> i32 {
	//Declare x to be a function pointer
	//that references a function with two i32 params
	//and returns an i32
	declare x:fn(i32, i32) -> i32;

	//This is the add function
	x := add;

	//Should call add on 1 and 3
	ret x(1, 3);
}
