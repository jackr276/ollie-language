/**
* Author: Jack Robbins
* This file will test the parser's ability to detect mismatches in parameter list length
*/

fn tester(x:i32, y:i32) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	//Too many parameters
	ret @tester(3, 4, 5);
}
