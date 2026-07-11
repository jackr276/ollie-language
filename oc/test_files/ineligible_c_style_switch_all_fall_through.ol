/**
 * Author: Jack Robbins
 * Test a switch switch that needs to be converted to an if-else statement because the
 * range is too large, where we have extensive fall-through
 */


 pub fn ineligible_c_style(x:i32) -> i32 {
	let result:mut i32 = 5;

 	switch(x) {
		default:
			result -= 55;

		case 5:
			result += 44;

		case 55555:
			result += 99;

		case -22222:
			result -= 11;
	}

	ret result;
 }


 pub fn main() -> i32 {
 	OUNIT: [exit_status = 82]
 	ret @ineligible_c_style(-222);
 }
