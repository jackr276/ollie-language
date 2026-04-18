/**
* Author: Jack Robbins
* Test some basic reordering of eligible expressions with constants
*/


pub fn main() -> i32 {
	let x:i32 = 5;

	/**
	* We should recognize the opportunity here and reorder
	* this into x * 4 + 5. They will both give the right
	* result of 25
	*/
	ret 5 + 4 * x;
}
