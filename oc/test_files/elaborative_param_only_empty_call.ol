/**
* Author: Jack Robbins
* Test the case where we have an elaborative param only in a function call *and* we decide
* not to populate it
*/

pub fn elaborative_param(x:params i32) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < paramcount(x); i++){
		result += x[i];
	}

	ret result;
}


pub fn main() -> i32 {
	//This is 100% valid - we have an elaborative param and we are opting not to pass anything
	ret @elaborative_param();
}
