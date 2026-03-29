/**
* Author: Jack Robbins
* Test our ability to declare public global variables
*/

declare pub global_var_1:mut i32;
let pub global_var_2:mut i32[] = [1, 2, 3, 4];


pub fn main() -> i32 {
	global_var_1 = 5;

	ret global_var_1 + global_var_2[2];
}
