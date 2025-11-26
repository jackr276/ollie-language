/**
* Author: Jack Robbins
* Test edge cases for uninitialized variable detection
*/


fn uninitialized_in_do_while_loop(arg:mut i32) -> void {
	//Declare but do not initialize
	declare c:mut i32;

	do{
		//Should fail - C is never initialized
		c = c + 3;
	} while(arg-- > 0);
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
