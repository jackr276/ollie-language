/**
* Author: Jack Robbins
* Test that we are able to handle the elaborative param with variable entries
* This test is specifically for indirect calls
*/

pub fn elaborative_param(x:i32, y:params i32) -> i32 {
	let result:mut i32 = x;

	for(let i:mut i32 = 0; i < paramcount(y); i++){
		result += y[i];
	}

	ret result;
}


pub fn main() -> i32 {
	let a:i32 = 5;
	let b:i32 = 6;
	let c:i32 = 7;
	let d:i32 = 8;
	let e:i32 = 9;

	// Test with a direct all
	ret @elaborative_param(1, a, b, c, d, e);
}
