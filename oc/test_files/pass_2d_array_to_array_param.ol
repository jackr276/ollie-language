/**
* Author: Jack Robbins
* Test passing a 2D array to a 2D array param. This should work and should allow us to get proper
* contiguous memory access
*/

pub fn take_2d_array(arr:i32[3][3]) -> i32 {
	ret arr[1][2];
}


pub fn main() -> i32 {
	let x:i32[][] = [[1, 2, 3], [1, 2, 3], [1, 5, 7]];
	
	ret @take_2d_array(x);
}
