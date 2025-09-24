/**
* Author: Jack Robbins
* Testing if assignment
*/

pub fn main(argc:i32, argv:char**) -> i32{
	declare mut x:i32;

	if(argc == 3) {
		x = 2;
	} else {
		x = 3;
	}

	ret x;
}
