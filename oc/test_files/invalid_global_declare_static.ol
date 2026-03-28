/**
* Author: Jack Robbins
* Test an invalid attempt to declare a static global variable
*/

//Invalid - you can't have a static global var
declare static x:i32;


pub fn main() -> i32 {
	x = 0;

	ret x;
}
