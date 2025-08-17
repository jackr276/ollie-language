/**
* Author: Jack Robbins
* This program is made to test C-style switch statements
*/

pub fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;
	let mut y:i32 := 16;

	switch(arg){
		case 11:
			x := 32;
			y *= 2;
			break;
		case 1:
			x := -3;
			while(x != 0){
				x -= 1;
			}
		case 4: 
			break when(y == 3);
		case 3:
			x := 211;
			break;

		case 6:
			x := 22;
			break;

		default:
			x := x - 22;
			break;
	}

	//So it isn't optimized away
	ret x;
}
