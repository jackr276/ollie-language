/**
* Author: Jack Robbins
* Testing global array variables
*/

declare array:mut i32[8][4];


pub fn main() -> i32 {
	array[0][0] = 232;
	array[1][1] = 11;
	array[2][2] = 28;
	array[3][3] = 28;
	array[4][3] = 29;
	array[5][2] = 30;
	array[6][1] = 31;
	array[7][1] = 32;

	for(let i:mut i32 = 0; i < 8; i++){
		array[i][i - 1] = i * 3 - 2;
	}

	ret 0;
}
