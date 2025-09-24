/**
* Author: Jack Robbins
* This program is made to test C-style switch statements
*/

pub fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 = 32;

	switch(arg){
		case 2:
			x = 32;
		case 1:
			x = -3;
		case 4: 
		case 3:
			x = 211;
			ret x + 1;

		case 6:
			x = 22;
			break;

		default:
			x = x - 22;
			ret x * 2;
	}

	//So it isn't optimized away
	ret x;
}
