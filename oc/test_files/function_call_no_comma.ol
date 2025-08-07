/**
* Author: Jack Robbins
* This file will test the parser's ability to detect syntax violations
* in function calls
*/

fn tester(x:i32, y:i32) -> i32 {
	ret x - y;
}


fn main() -> i32 {
	//No comma between parameters
	ret @tester(3 4);
}
