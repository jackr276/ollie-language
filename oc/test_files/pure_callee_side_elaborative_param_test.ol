/**
* Author: Jack Robbins
* This test file exists just to go through and test the callee side of the elaborative param process, specifically
* our access using the array accessor
*/

pub fn elaborative_param(x:i32, y:params i32) -> i32 {
	let result:mut i32 = x;

	for(let i:mut i32 = 0; i < paramcount(y); i++){
		result += y[i];
	}

	ret result;
}

//Pure dummy
pub fn main() -> i32 {
	ret 0;
}
