/**
 * Author: Jack Robbins
 * Test a more extreme case of initializing an immutable variable
 */


pub fn initialize_immutable_switch(x:i32) -> i32 {
	declare result:i32;

	switch(x){
		case 1 -> {
			result = 5;
		}

		case 3 -> {
			result = 7;
		}

		case 5 -> {
			result = 11;
		}

		default -> {
			result = 2;
		}
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 7]
	ret @initialize_immutable_switch(3);
}
