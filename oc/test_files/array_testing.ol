/**
* This program is made for the purposes of testing arrays
*/

fn main(arg:i32, argv:char**) -> i32{
	//The array that we have
	declare mut arr:i32[14][2];
	declare mut oneD:i64[2];
	
	arr[3][1] := 3;
	arr[5][1] := 3;
	arr[7][0] := 2;

	oneD[1] := 3;

	let mut i:i32 := 3;
	//let mut j:i32* := arr;
	//j := j + 1;
	
	if(arg == 2) then {
		arr[2][1] := 3;
	} else {
		i := arr[0][0];
	}



	//So it isn't optimized away
	ret arr[7][2];// + *(j+1);
}
