/**
* Author: Jack Robbins
* This test file aims to test the functionality of returning a function pointer
*/

//Define an arithmetic function pointer that takes in two i32's
define fn(mut i32, i32) -> i32 as arithmetic_function;

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

/**
* Shares the same signature as add
*/
fn multiply(mut x:i32, y:i32) -> i32{
	ret x * y;
}

/**
* Chooser takes in a variable and returns a function signature
*/
fn choose_function(x:i32) -> arithmetic_function{
	if(x == 0){
		ret multiply;
	} else if(x == 1){
		ret subtract;
	} else {
		ret add;
	}
}


pub fn main() -> i32{
	let x:arithmetic_function = @choose_function(2);

	ret @x(1, 3);
}


