/**
* Author: Jack Robbins
* Test our ability to initialize global variables with multi-dimensional arrays
*/

let global_arr:i32[][] = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];


pub fn main() -> i32 {
	ret global_arr[2][2];
}
