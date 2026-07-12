/**
 * Author: Jack Robbins
 * Test an in statement that is ineligible to be a switch internally by reason of too few
 * members(less than 3)
 */

pub fn ineligible_in(x:i32) -> i32 {
	let result:mut i32 = 15;

	if(x in (5, 7)) {
		result += 5;
	} else {
		result -= 11;
	}

	ret result;
}


pub fn main() -> i32 {
	//Should return 20 + 4 = 24
	OUNIT: [exit_status = 24]
	ret @ineligible_in(7) + @ineligible_in(15);
}
