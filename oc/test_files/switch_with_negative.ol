/**
* Author: Jack Robbins
* Test the use of a switch statement with negatives
*/


pub fn switch_with_negatives(x:i32) -> i32 {
	switch(x) {
		case -11 -> {
			ret 5;
		}

		case 0 -> {
			ret 1;
		}

		case -5 -> {
			ret 0;
		}

		case 12 -> {
			ret 5;
		}

		default -> {
			ret -1;
		}
	}
}


pub fn main() -> i32 {
	OUNIT: [console = 5]
	ret @switch_with_negatives(-11);
}
