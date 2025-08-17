/**
* Author: Jack Robbins
* This file tests handling of a main function with an invalid length
*/


/**
* Should lead to a failure because we can only have 2 or 0 arguments
*/
pub fn main(argc:i32, argv:char**, x:i32) -> i32{
	ret **argv + argc + x;
}
