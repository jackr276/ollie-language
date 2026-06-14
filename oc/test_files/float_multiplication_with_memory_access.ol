/**
* Author: Jack Robbins
* Basic tester for floating point multiplication with memory access
*/


pub fn multiply_floats(arr:f32[10], x:f32) -> f32 {
	ret x * arr[5];
}


pub fn main() -> i32 {
	let x:f32[10] = [1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.10];

	//Should return 5.5 * 6.6 = 36.6
	OUNIT: [console = 36]
	ret @multiply_floats(x, 5.5);
}
