/**
* This program is made for the purposes of testing arrays
*/

fn main(arg:i32, argv:char**) -> i32{
	define construct my_struct{
		mut ch:char;
		mut x:i32[80];
		mut lch:char;
		mut y:i32;

	} as my_struct;

	declare structure:my_struct;

	structure:ch := 'a';
	structure:x[3] := 3;
	structure:x[5] := 2;
	structure:lch := 'b';
	structure:y := 5;

	//So it isn't optimized away
	ret structure:x[2];//x[2];// +*/ structure:lch;
}
