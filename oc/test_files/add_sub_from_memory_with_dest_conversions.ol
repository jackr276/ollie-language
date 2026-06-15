/**
* Author: Jack Robbins
* Test adding/subtracting from memory with destination conversions to test how we handle it
*/


pub fn add_with_dest_conversion(arr:i16[5], x:i16) -> i32 {
	ret x + arr[3];
}

pub fn sub_with_dest_conversion(arr:i16[5], x:i16) -> i32 {
	ret x - arr[4];
}


pub fn main() -> i32 {
	let arr:i16[5] = [1, 2, 3, 4, 5];

	//Should return 10 + 4 + 10 - 5 = 19
	OUNIT: [console = 19]
	ret @add_with_dest_conversion(arr, 10) + @sub_with_dest_conversion(arr, 10);
}
