/**
* Author: Jack Robbins
* Testing if assignment
*/

pub fn main(argc:i32, argv:char**) -> i32{
	declare x:mut i32;

	if(argc == 3) {
		x = 2;
	} else {
		x = 3;
	}

	ret x;
}
