/**
 * Author: Jack Robbins
 * Test the case where we have an in statement that exceeds the maximum switch bounds
 */


pub fn over_max_bounds(x:i32) -> i32 {
	if(x in (1000, 0, -3, 55)) {
		ret 50;
	} else {
		ret 100;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 100]
	ret @over_max_bounds(54);
}
