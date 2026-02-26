/**
* Author: Jack Robbins
* Test the register allocator's ability to intelligently merge stack deallocation
* statements if they're right next to eachother
*/


pub fn more_than_6(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:i32) -> i32 {
	ret x + y + z * aa - bb + cc - dd;
}


pub fn main() -> i32 {
	let arr:i32[] = [1, 2, 3, 4, 5, 6, 7];

	ret @more_than_6(arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6]);
}
