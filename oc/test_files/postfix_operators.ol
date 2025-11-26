/**
* Author: Jack Robbins
* Testing of basic postfix operator functionality
*/

pub fn main(arg:i32, argv:char**) -> i32 {
	//The array that we have
	declare arr:mut i32[14][2];
	declare oneD:mut i64[2];
	
	arr[3][1] = 3;
	arr[5][2] = 3;
	arr[7][0] = 2;

	oneD[1] = 3;

	let x:mut i32 = 33;

	let i:mut i32 = 3;
	
	if(arg == 2) {
		arr[x][1] = 3;
	} else {
		i = arr[0][0];
	}

	//So it isn't optimized away
	ret arr[7][2]--;
}
