/**
* Author: Jack Robbins
* Test the use of the elaborative param 'params' in ollie
*/


/**
* We can use the elaborative params type to pass in the number of
* parameters to a given function. This is effectively a "void*"
* under the hood because any type may be used
*/
pub fn elaborative_param(x:i32, y:i32, params elaborative_params) -> i32 {
	let result:i32 = x + y;

	for(let i:i32 = 0; i < paramsize(elaborative_params, ))

	
}


pub fn main() -> i32 {
	//Here 4, 5 and 6 would go onto the stack
	ret @elaborative_param(1, 2, 4, 5, 6);
}
