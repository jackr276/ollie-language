/**
 * Author: Jack Robbins
 * Test an ollie switch that needs to be converted to an if-else statement because the
 * range is too large
 */


 pub fn ineligible_c_style(x:i32) -> i32 {
	let result:mut i32 = 5;

 	switch(x) {
		default:
			result = 55;
			break;

		case 5:
			result = 44;
			break;

		case 55555:
			result = 99;
			break;

		case -22222:
			result = 500;
			break;
	}

	ret result;
 }


 pub fn main() -> i32 {
 	OUNIT: [exit_status = 99]
 	ret @ineligible_c_style(55555);
 }
