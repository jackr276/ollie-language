/**
* Author: Jack Robbins
* Basic mutability checking for simple values
*/


pub fn main() -> i32 {
	declare x:i32;

	//This is invalid, we cannot assign x here
	x = 5;

	ret x;
}
