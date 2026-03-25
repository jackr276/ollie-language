/**
* Author: Jack Robbins
* This is a stress test of our system where we have a function that not only has stack params(more than 6 GP), but
* also has an elaborative param.
*/


/**
* Here g and param_arr are both on the stack
*/
pub fn stress_test_elaborative(a:i32, b:i32, c:i32, d:char, e:char, f:char, g:i16, param_arr:params i32) -> i32 {
	let result:mut i32 = a + b + c + d + e + f + g;

	for(let i:mut i32 = 0; i < paramcount(param_arr); i++) {
		result += param_arr[i];
	}
	
	ret result;
}


pub fn main() -> i32 {
	let x:i32 = 3;
	let y:i32 = x + 1;
	let z:i32 = y - 2;

	//Make the call. x,y,z is the elaborative portion here
	ret @stress_test_elaborative(1, 2, 3, 4, 5, 6, 7, x, y, z);
}
