/**
 * Author: Jack Robbins
 * Test an invalid case where we attempt to overload an existing function with an elaborative param
 * function. Ollie strictly forbids this because it would cause too much ambiguity
 */


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn add(x:params i32) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < paramcount(x); i++){
		result += x[i];
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
