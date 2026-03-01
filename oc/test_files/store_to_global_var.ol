/**
* Author: Jack Robbins
* Test storing to a global variable
*/

let global_x:mut i32 = 3;


pub fn main() -> i32 {
	global_x = 5;

	ret global_x;
}
