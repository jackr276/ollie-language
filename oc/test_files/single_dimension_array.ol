/**
* This program is made for the purposes of testing arrays
*/

fn main(arg:i32, argv:char**) -> i32{
	//The array that we have
	declare mut arr:i32[14];

	arr[3] := 3;
	arr[5] := 3;
	arr[7] := 2;

	let mut x:i32 := 33;

	let mut i:i32 := 3;
	
	if(arg == 2) then {
		arr[x] := i;
	} else {
		i := arr[0];
	}

	//So it isn't optimized away
	ret arr[i];
}
