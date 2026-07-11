/**
 * Author: Jack Robbins
 * Test an ollie switch statement whose range exceeds the range for switch
 * statements and is therefore forced to be an if-else internally
 */


 pub fn ineligible_ollie_negative(x:i32) -> i32 {
 	let result:mut i32 = 5;

	switch(x) {
		case -1000 -> { 
			result *= 5;
		}

		case -55555 -> {
			result += 22;
		}

		default -> {
			result--;
		}

		case -2 -> {
			result *= 10;
		}
	}

	ret result;
 }


 pub fn main() -> i32 {
 	OUNIT: [exit_status = 25]
 	ret @ineligible_ollie_negative(-1000);
 }
