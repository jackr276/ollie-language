/**
* Author: Jack Robbins
* This file will test the parser's ability to detect syntax violations
* in function calls
*/

fn tester(x:i32, y:i32) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	//Invalid extra comma
	ret @tester(3, 4,);
}
