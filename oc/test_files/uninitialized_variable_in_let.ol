/**
* Author: Jack Robbins
* Test edge cases for uninitialized variable detection
*/

//Dummy
pub fn main() -> i32 {
	//This should fail
	let c:mut i32 = c + 3;

	OUNIT: [fail_to_compile]
	ret 0;
}
