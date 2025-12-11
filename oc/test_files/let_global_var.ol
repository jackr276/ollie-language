/**
* Author: Jack Robbins
* Test the use of "let" with global vars
*/

//Global var
let x:mut i32 = 33;
//Mix and match - we should handle this
declare y:i32;


pub fn main() -> i32 {	
	y = 22;
	ret x + 11 + y;
}
