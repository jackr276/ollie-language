/**
* Author: Jack Robbins
* Test the case where we have a global string array that is 2 dimensional
*/

let x:char*[] = ["Hi", "Ok", "No"];

pub fn main() -> i32 {
	ret x[2][1];
}
