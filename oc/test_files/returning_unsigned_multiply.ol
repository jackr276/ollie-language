/**
* Author: Jack Robbins
* Test the case where we are returning an unsigned multiplication instruction here. We need
* to ensure that we are not generating extra instructions if we do this
*/


pub fn return_unsigned_multiply(x:u32, y:u32) -> u32 {
	ret x * y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
