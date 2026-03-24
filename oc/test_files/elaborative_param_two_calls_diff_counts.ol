/**
* Author: Jack Robbins
* The entire point of the elaborative param is to be able to use different parameter
* list lengths for different function calls. This test file tests that functionality for a
* direct function call
*/


pub fn elaborative_param(x:i32, y:params i32) -> i32 {
	let result:mut i32 = x;
	let count:mut i32 = paramcount(y);

	let i:mut i32 = 0;

	while(i < count) {
		result += y[i];
		
		i++;
	}

	ret result;
}


//Invoke the elaborative param with two separate function lists
pub fn invoke_elaborative() -> i32 {
	let x:i32 = @elaborative_param(1, 2, 3);
	let y:i32 = @elaborative_param(1, 4, 5, 6, 7, 8);
	
	ret x + y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
