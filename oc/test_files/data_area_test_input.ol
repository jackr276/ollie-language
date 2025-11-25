/**
* This program is made for the purposes of testing arrays
*/

pub fn main(arg:mut i32, argv:char**) -> i32{
	//The array that we have
	declare arr:mut i32[14][17];
	declare oneD:mut i64[2];
	declare oneDi32:mut i32[2];
	
	define struct my_struct{
		ch:mut char;
		x:mut i64;
		lch:mut char;
		y:mut i32;

	} as my_struct;

	declare structure:mut my_struct;

	structure:ch = 'a';
	structure:x = 3;
	structure:lch = 'b';
	structure:y = 5;

	if(arg == 0) {
		structure:x = 2;
		structure:y = 1;
		structure:lch = 'c';
	} else{
		structure:y = 5;
	}

	//Useless assign
	let z:mut i64 = structure:x;

	//structure:x = 7;
	let x:i64 = structure:x;

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
	ret arr[7][x] + oneDi32[2] + structure:x;
}
