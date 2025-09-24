/**
* Author: Jack Robbins
* This test will detect the ability to deal with bad 
* types in function calls
*/

fn tester(x:i32, y:i8) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	let mut x:i32 = 6;

	//Bad type, x is too large
	ret @tester(3, x);
}
