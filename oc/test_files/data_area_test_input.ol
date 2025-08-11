/**
* This program is made for the purposes of testing arrays
*/

fn main(mut arg:i32, argv:char**) -> i32{
	//The array that we have
	declare mut arr:i32[14][17];
	declare mut oneD:i64[2];
	declare mut oneDi32:i32[2];
	
	define construct my_struct{
		mut ch:char;
		mut x:i64;
		mut lch:char;
		mut y:i32;

	} as my_struct;

	declare mut structure:my_struct;

	structure:ch := 'a';
	structure:x := 3;
	structure:lch := 'b';
	structure:y := 5;

	if(arg == 0) {
		structure:x := 2;
		structure:y := 1;
		structure:lch := 'c';
	} else{
		structure:y := 5;
	}

	//Useless assign
	let mut z:i64 := structure:x;

	//structure:x := 7;
	let x:i64 := structure:x;

	arr[3][1] := 3;
	arr[5][arg] := 3;
	arr[7][0] := 2;
	arr[arg][3] := 2;

	oneDi32[1] := 3;

	let mut x:i32 := 33;
	x := arr[arg][13];

	let mut i:i32 := 3;
	//let mut j:i32* := arr;
	//j := j + 1;
	arr[i][2] := 333;
	
	if(arg == 2) {
		arr[x][i] := arr[2][x];
	} else {
		i := arr[0][0];
	}

	//So it isn't optimized away
	ret arr[7][x] + oneDi32[2] + structure:x;
}
