/**
* Author: Jack Robbins
* Test the edge case where we have an elaborative param in the signature
* but we elect not to put anything. This is a valid case and must be handled
*/

pub fn elaborative_param(x:i32, y:params i32) -> i32 {
	let result:mut i32 = x;

	for(let i:mut i32 = 0; i < paramcount(y); i++){
		result += y[i];
	}

	ret result;
}


pub fn main() -> i32 {
	//Force to a function pointer
	let my_func:fn(i32, params i32) -> i32 = elaborative_param;

	//Totally valid case where we aren't using anything
	ret @my_func(0);
}
