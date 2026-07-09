/**
* Author: Jack Robbins
* Basic tester for unsigned multiplication with memory access
*/


pub fn multiply_unsigned(arr:u32[10], x:u32) -> u32 {
	ret x * arr[5];
}


pub fn main() -> i32 {
	let x:u32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Should return 5 * 6 = 30
	OUNIT: [exit_status = 30]
	ret @multiply_unsigned(x, 5);
}
