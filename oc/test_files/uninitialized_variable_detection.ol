/**
* Author: Jack Robbins
* Test edge cases for uninitialized variable detection
*/

fn uninitialized_in_while_loop(mut arg:i32) -> void {
	let mut c:i32 = 3;

	while(arg-- > 0){
		//Should fail - C is never initialized
		c = c + 3;
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
