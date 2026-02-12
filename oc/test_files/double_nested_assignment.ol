/**
* Author: Jack Robbins
* A bit of an absurd case, but in theory it should be supported
* so we will test it
*/

pub fn main() -> i32 {
	declare x:mut i32, declare y:mut i32;

	
	ret (x = 5) + (y = 6);
}
