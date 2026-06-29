/**
* Author: Jack Robbins
* Test a case where we have an ollie switch that only has one member, and the default case is
* what we intend on hitting
*/


pub fn main() -> i32 {
	let x:mut i32 = 4;

	switch(x){
		case 5 -> {
			x++;
		}

		default -> {
			x--;
		}
	}

	OUNIT: [console = 3]
	ret x;
}
