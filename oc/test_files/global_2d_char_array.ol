/**
* Author: Jack Robbins
* Add testing for a global 2D char array. This is supported in ollie
*/

let global_char_arr:char[][] = ["Hi", "Oh", "No"];

pub fn main() -> i32 {
	ret global_char_arr[2][0];
}
