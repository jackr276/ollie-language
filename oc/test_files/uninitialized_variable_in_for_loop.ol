/**
* Author: Jack Robbins
* Test edge cases for uninitialized variable detection
*/

fn uninitialized_in_for_loop(arg:mut i32) -> void {
	//Declare but do not initialize
	declare c:i32;

	for(let i:mut i16 = 3; i < arg; i++) {
		//Should fail - C is never initialized
		c = c + 3;
	} 
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
