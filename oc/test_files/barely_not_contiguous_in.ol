/**
 * Author: Jack Robbins
 * Put our contiguous in detection to the test by having one that's just on the edge of being contiguous
 */

pub fn barely_not_contiguous_in(x:i32) -> i32 {
	let result:mut i32 = 5;

	//Almost there but not quite - there's a gap between -1 and 1
	if(x in (-1, 2, 3, 1)) {
		result *= 10;
	} else {
		result -= 2;
	}

	ret result;
}

pub fn main() -> i32 {
	OUNIT: [exit_status = 50]
	ret @barely_not_contiguous_in(3);
}
