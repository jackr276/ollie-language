/**
* Author: Jack Robbins
* This file will test the use of function pointers
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


pub fn main() -> i32 {
	//Define an arithmetic function pointer that takes in two i32's
	define fn(mut i32, i32) -> i32 as arithmetic_function;

	//This is the add function
	let x:mut arithmetic_function = add;
	let y:mut arithmetic_function = subtract;

	//A more complex example
	let a:mut i32 = @x(1, 3) + @y(2, 7);
	
	ret a;
}
