/**
* Author: Jack Robbins
* Test the case where the user declares an invalid 2d char array
*/

//These are all different lengths - should not work
let x:char[][] = ["Hi", "Hello", "Hey"];

pub fn main() -> i32 {
	ret x[0][0];
}
