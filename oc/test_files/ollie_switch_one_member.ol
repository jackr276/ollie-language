/**
* Author: Jack Robbins
* Test a case where we have an ollie switch that only has one member
*/

pub fn ollie_switch_one_member(x:mut i32) -> i32 {
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
	OUNIT: [exit_status = 6]
	ret @ollie_switch_one_member(5);
}
