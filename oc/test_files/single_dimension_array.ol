/**
* This program is made for the purposes of testing arrays
*/

pub fn main(arg:i32, argv:char**) -> i32{
	//The array that we have
	declare arr:mut i32[14];

	arr[3] = 3;
	arr[5] = 3;
	arr[7] = 2;

	let x:mut i32 = 33;

	let i:mut i32 = 3;
	
	if(arg == 2){
		arr[x] = i;
	} else {
		i = arr[0];
	}

	//So it isn't optimized away
	ret arr[i];
}
