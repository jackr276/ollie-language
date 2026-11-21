/**
* Author: Jack Robbins
* Test a case where we have a c switch that will, after the middle end runs,
* get optimized into a switch with only one case statement, and see how we handle it
*/

pub fn c_switch_optimized(x:mut i32, y:mut i32) -> i32 {
	switch(x) {
		case 5:
			x++;
			break;

		//Useless will be scrapped
		case 4:
			y--;
			break;

		//Useless will be scrapped
		case 3:
			y++;
			break;
		
		//Do nothing
		default:
			break;
	}

	//Only x is useful, y will be scrapped
	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @c_switch_optimized(5, 4);
}

