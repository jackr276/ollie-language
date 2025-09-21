/**
* Author: Jack Robbins
* This file is intended to test structure intializers
*/


/**
* Size should be: 4 + 4 pad + 8 + 1 + 7 pad = 24 (end must be a multiple of 8)
*/
define struct custom {
		x:i32;
		a:i64;
		y:char;
} as my_struct;


pub fn main(arg:i32, argv:char**) -> i32{
	declare mut x:i32;

	switch(arg){
		case 2 -> {
			x := 32;
		}
		case 1 -> {
			x := 3;
		}
		case 7 -> {
			x := 2;
		}
		// Empty default
		default -> {}
	}

	//Very basic initializer
	let tester:my_struct := {x, 7, 'a'};

	//preincrement this
	++tester:x;

	//So it isn't optimized away
	ret tester:x;
}
