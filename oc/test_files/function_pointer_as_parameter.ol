/**
* Author: Jack Robbins
* Test the use of function pointers as parameters
*/

//Top level type
define fn(mut i32, i32) -> i32 as arithmetic_function;

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
* Makes a call using a pointer
*/
fn function_call_wrapper(func_ptr:arithmetic_function, x:i32) -> i32 {
	//Some nonsense
	let y:mut i32 = 3;
	y += (x > 3) ? x else 4;

	ret @func_ptr(x, y);
}


//Makes the call to the helper
pub fn main() -> i32 {
	ret @function_call_wrapper(add, 0);
}
