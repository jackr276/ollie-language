/**
* Author: Jack Robbins
* This file tests to/from memory converting moves
*/


pub fn tester(a:i64, ptr:i32*) -> i64 {
	ret a + *ptr;
}



pub fn main() -> i32 {
	declare mut arr:i32[30];
	
	//Should trigger converting store
	let ex:char = 'c';
	arr[3] = ex;

	//Should trigger an upwards converting move
	let mut y:i64 = arr[5];

	//Just call for fun
	@tester(y, arr);

	ret arr[2];
}
