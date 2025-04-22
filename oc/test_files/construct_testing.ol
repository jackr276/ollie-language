/**
* This program is made for the purposes of testing case statements
*/

fn main(arg:i32, argv:char**) -> i32{
	define construct my_struct{
		mut ch:char;
		mut x:i32;
		mut lch:char;
		mut y:i32;

	} as my_struct;

	declare structure:my_struct;

	structure:ch := 'a';
	structure:x := 3;
	structure:lch := 'b';
	structure:y := 5;

	//structure:x := 7;
	let x:i32 := 3;

	//So it isn't optimized away
	ret x;
}
