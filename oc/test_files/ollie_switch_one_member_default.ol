/**
* Author: Jack Robbins
* Test a case where we have an ollie switch that only has one member, and the default case is
* what we intend on hitting
*/


pub fn switch_one_member(x:mut i32) -> i32 {
	switch(x){
		case 5 -> {
			x++;
		}

		default -> {
			x--;
		}
	}

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 3]
	ret @switch_one_member(4);
}
