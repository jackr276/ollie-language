/**
 * Author: Jack Robbins
 * Test an invalid case where we attempt to overload a function that contains an elaborative stack param.
 * Ollie forbids this because it would cause too much call site ambiguity
 */


declare pub fn add(params i32) -> i32;
declare pub fn add(i32, i32) -> i32;


pub fn add(x:params i32) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < paramcount(x); i++){
		result += x[i];
	}

	ret result;
}


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
