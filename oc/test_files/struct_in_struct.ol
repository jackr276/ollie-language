/**
* Author: Jack Robbins
* This file is intended to test structure intializers
*/


/**
* Should be 24 in size(4 + 4 pad + 8 + 1 + 3 pad + 4 pad(multiple of 8) = 24)
*/
define struct custom {
		x:i32;
		a:i64;
		y:char;
} as my_struct;

/**
* Should be  (24 + 8 + 4 + 1 + 3 pad = 40)
*/
define struct parent {
	internal:mut my_struct;
	x:mut i64;
	y:mut i32;
	z:mut char;
} as parent_struct;


pub fn main(arg:i32, argv:char**) -> i32{
	declare parent:mut parent_struct;
	declare internal_struct:mut my_struct;

	switch(arg){
		case 2 -> {
			parent:internal:x = 32;
			internal_struct:x = 322;
		}
		case 1 -> {
			parent:internal:x = 32;
		}
		case 7 -> {
			parent:internal:y = 32;
		}

		// Empty default
		default -> {}
	}

	//So it isn't optimized away
	ret parent:internal:x + parent:y + internal_struct:x;
}
