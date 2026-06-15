/**
* Author: Jack Robbins
* Basic tester for signed multiplication with memory access
*/


pub fn multiply_signed(arr:i32[10], x:i32) -> u32 {
	ret x * arr[5];
}


pub fn main() -> i32 {
	let x:i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Should return 5 * 6 = 30
	OUNIT: [console = 30]
	ret @multiply_signed(x, 5);
}
