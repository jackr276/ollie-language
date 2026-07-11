/**
 * Author: Jack Robbins
 * Test an ollie switch statement whose range exceeds the range for switch
 * statements and is therefore forced to be an if-else internally
 */


 pub fn ineligible_ollie_negative(x:i32) -> i32 {
 	let result:mut i32 = 5;

	switch(x) {
		case -1000:
			result *= 5;
			//Fall through

		case -55555:
			result += 22;
			break;

		default:
			result--;
			//Fall through

		case -2:
			result *= 10;
			//Fall through
	}

	ret result;
 }


 pub fn main() -> i32 {
 	OUNIT: [exit_status = 40]
 	ret @ineligible_ollie_negative(-999);
 }
