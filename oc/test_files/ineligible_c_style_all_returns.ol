/**
 * Author: Jack Robbins
 * Test an ineligible c-style switch that has all returns and no falling through
 */

pub fn ineligible_switch(x:u32) -> i32 {
	switch(x) {
		case 2U:
			ret 55;

		case 55555:
			ret 78;

		default:
			ret 77;

		case 3:
			ret 15;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 15]
	ret @ineligible_switch(3U);
}
