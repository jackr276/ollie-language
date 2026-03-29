/**
* Author: Jack Robbins
* Test an invalid attempt to declare a static global variable using let
*/

//Invalid - you can't have a static global var
let static x:mut i32 = 5;


pub fn main() -> i32 {
	x = 0;

	ret x;
}
