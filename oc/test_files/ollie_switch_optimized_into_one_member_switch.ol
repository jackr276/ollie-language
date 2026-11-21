/**
* Author: Jack Robbins
* Test a case where we have an ollie switch that will, after the middle end runs,
* get optimized into a switch with only one case statement, and see how we handle it
*/


pub fn optimized_switch(x:mut i32, y:mut i32) -> i32 {
	switch(x) {
		case 5 -> {
			x++;
		}

		//Useless will be scrapped
		case 4 -> {
			y--;
		}

		//Useless will be scrapped
		case 3 -> {
			y++;
		}
		
		//Do nothing
		default -> {}
	}

	//Only x is useful, y will be scrapped
	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @optimized_switch(5, 4);
}
