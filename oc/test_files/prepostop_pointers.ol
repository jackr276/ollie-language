/**
* Author: Jack Robbins
* Test a bad postfix operator use
*/

pub fn main(arg:i32, argv:char**) -> i32 {
	//The array that we have
	declare mut arr:i32[14][2];
	declare mut oneD:i64[2];
	
	arr[3][1] := 3;
	arr[5][2] := 3;
	arr[7][0] := 2;

	oneD[1] := 3;

	let mut x:i32 := 33;

	let mut i:i32 := 3;
	
	if(arg == 2) {
		arr[x][1] := 3;
	} else {
		i := arr[0][0];
	}

	let mut ptr:i32* := arr[0];

	//(ptr++)[3] := 7;
	
	ret (++ptr)[2];
}
