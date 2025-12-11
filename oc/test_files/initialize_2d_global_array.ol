/**
* Author: Jack Robbins
* Test using a 2d array initializer for global variables
*/

//2d global array
let x:i32[][] = [[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12]];


pub fn main() -> i32 {
	ret x[2][3];
}
