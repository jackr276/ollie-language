/**
* Author: Jack Robbins
* This file will test the parser's ability to detect mismatches in parameter list length
*/

fn tester() -> i32 {
	ret 5;
}


pub fn main() -> i32 {
	let ptr:fn() -> i32 = tester;

	//Too many parameters
	OUNIT: [fail_to_compile]
	ret @ptr(5, 6, 7);
}
