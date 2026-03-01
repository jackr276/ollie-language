/**
* Author: Jack Robbins
* Test the unique case where we are passing an array as a stack variable in here and using it
*/


pub fn more_than_6(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, arr:i32[7]) -> i32 {
	ret x + y + z * aa - bb + cc - arr[1] + arr[2] + arr[3];
}


pub fn main() -> i32 {
	let arr:i32[] = [1, 2, 3, 4, 5, 6, 7];

	ret @more_than_6(1, 2, 3, 4, 5, 6, arr);
}
