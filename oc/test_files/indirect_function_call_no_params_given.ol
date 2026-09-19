/**
* Author: Jack Robbins
* This file will test the parser's ability to detect mismatches in parameter list length
*/

fn tester(x:i32, y:i32) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	let ptr:fn(i32, i32) -> i32 = tester;

	//Too few parameters
	OUNIT: [fail_to_compile]
	ret @ptr();
}
