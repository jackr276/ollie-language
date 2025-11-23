/**
* Author: Jack Robbins
* Test indirect function calls that return void
*/


/**
* Shares the same signature as subtract
*/
fn add(x:mut i32*, y:i32) -> void {
	*x = *x + y;
}

/**
* Shares the same signature as add
*/
fn subtract(x:mut i32*, y:i32) -> void {
	*x = *x - y;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = 3;

	//Define an arithmetic function pointer that takes in two i32's
	define fn(mut i32*, i32) -> void as arithmetic_function;

	//This is the add function
	let test:mut arithmetic_function = add;

	//Call indirectly
	@test(&x, argc);
	
	ret x + argc;
}
