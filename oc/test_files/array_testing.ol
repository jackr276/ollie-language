/**
* This program is made for the purposes of testing arrays
*/

pub fn main(arg:mut i32, argv:char**) -> i32{
	//The array that we have
	declare arr:mut i32[14][17];
	declare oneD:mut i64[2];
	declare oneDi32:mut i32[2];
	
	arr[3][1] = 3;
	arr[5][arg] = 3;
	arr[7][0] = 2;
	arr[arg][3] = 2;

	oneDi32[1] = 3;

	let x:mut i32 = 33;
	x = arr[arg][13];

	let i:mut i32 = 3;
	arr[i][2] = 333;
	
	if(arg == 2) {
		arr[x][i] = arr[2][x];
	} else {
		i = arr[0][0];
	}



	//So it isn't optimized away
	ret arr[7][x] + oneDi32[2];// + *(j+1);
}
