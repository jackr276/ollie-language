/**
* Author: Jack Robbins
* Test handling for complex case statements
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	switch(argc){
		case 3 + 3:
			ret 7;

		case 4 + typesize(char**):
			ret 5;

		case sizeof(argv):
			ret 3;

		default:
			ret 0;
	}
}
