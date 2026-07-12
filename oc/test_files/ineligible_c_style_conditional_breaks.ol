/**
 * Author: Jack Robbins
 * Test how an ineligible c-style is handled when we have conditional breaks
 */

 pub fn ineligible_c_style(x:i32, y:i32) -> i32 {
 	let result:mut i32 = 11;

	switch(x) {
		case -5555:
			break when (y in (1, 2));
		case 27:
			result += y;
			break;

		case 800:
			break when (y > 5);
			result += 5;

		case 999:
			result *= 3;
			break;

		default:
			break;
	}

	ret result;
 }


 pub fn main() -> i32 {
 	//Should return 14 + 48 = 62
	OUNIT: [exit_status = 62]
 	ret @ineligible_c_style(-5555, 3) + @ineligible_c_style(800, 5);
 }
