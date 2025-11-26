/**
* Author: Jack Robbins
* This file tests to/from memory converting moves
*/


pub fn tester(a:i64, ptr:i32*) -> i64 {
	ret a + *ptr;
}


pub fn tester2(a:i64, ptr:i32*) -> i64 {
	ret *ptr + a;
}

pub fn expanding_load(ptr:i32*) -> i64 {
	//Should trigger expansion
	ret *ptr;
}

pub fn expanding_pointer_tester(ptr:mut i64*, x:i32) -> i32 {
	//Should trigger expansion
	*ptr = x;

	ret x;
}

pub fn main() -> i32 {
	declare arr:mut i32[30];
	
	//Should trigger converting store
	let ex:char = 'c';
	arr[3] = ex;

	//Should trigger an upwards converting move
	let y:mut i64 = arr[5];

	//Just call for fun
	@tester(y, arr);

	ret arr[2];
}
