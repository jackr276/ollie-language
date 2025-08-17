/**
* Author: Jack Robbins
* This test will detect the ability to deal with bad 
* types in function calls
*/

fn tester(x:i32, y:i8) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	//Define the type
	define fn(i32, i8) -> i32 as test_func;

	//This one is tester
	let my_func:test_func := tester;

	let mut x:i32 := 6;

	//Bad type, x is too large
	ret @my_func(3, x);
}
