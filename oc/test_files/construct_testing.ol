/**
* This program is made for the purposes of testing case statements
*/

fn main(arg:i32, argv:char**) -> i32{
	define construct my_struct{
		mut x:i32;
		mut y:i32;
		mut z:f64;

	} as my_struct;

	declare structure:my_struct;

	structure:x := 3;
	structure:y := 5;
	structure:z := 3.0;

	let x:i32 := 3;

	//So it isn't optimized away
	ret x;
}
