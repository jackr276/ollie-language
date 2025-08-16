/**
* This program is made for the purposes of testing case statements
*/

fn not_main(arg:i32, argv:char**) -> i64 {
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

	//So it isn't optimized away
	ret x;
}


pub fn main(arg:i32, argv:char**) -> i32{
	@not_main(arg, argv);
	ret arg;
}
