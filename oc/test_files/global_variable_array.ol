/**
* Author: Jack Robbins
* Testing global array variables
*/

declare mut array:i32[8];


pub fn main() -> i32 {
	array[0] = 232;
	array[1] = 11;
	array[2] = 28;
	array[3] = 28;
	array[4] = 29;
	array[5] = 30;
	array[6] = 31;
	array[7] = 32;

	for(let mut i:i32 = 0; i < 8; i++){
		array[i] = i * 3 - 2;
	}

	ret 0;
}
