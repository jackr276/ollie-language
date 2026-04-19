/**
* Author: Jack Robbins
* Investigate whether or not we are able to accurately value number
* conditional expressions
*/

pub fn value_number_conditionals(x:i32) -> i32 {
	//Should get value numbered
	let result1:i32 = x > 2;
	let result2:i32 = x > 2;

	//Should also get value numbered
	let final_result:i32 = result1 + result2;
	let final_result2:i32 = result1 + result2;

	ret final_result + final_result2 * 2;
}


pub fn main() -> i32 {
	//Should in theory return 2
	ret @value_number_conditionals(5);
}
