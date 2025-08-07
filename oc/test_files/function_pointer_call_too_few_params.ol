/**
* Author: Jack Robbins
* This file will test the parser's ability to detect mismatches in parameter list length
*/

fn tester(x:i32, y:i32) -> i32 {
	ret x - y;
}


fn main() -> i32 {
	//Define the type
	define fn(i32, i32) -> i32 as test_func;

	//This one is tester
	let my_func:test_func := tester;

	//Too few parameters
	ret @my_func(3);
}
