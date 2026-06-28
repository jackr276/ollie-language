/**
* Author: Jack Robbins
* Testing if assignment with variables
*/

pub fn main(argc:i32, argv:char**) -> i32{
	let a:i32 = 5;
	let b:i32 = 4;

	declare x:mut i32;

	if(argc == 3) {
		x = b;
	} else {
		x = a;
	}

	OUNIT: [console = 5]
	ret x;
}
