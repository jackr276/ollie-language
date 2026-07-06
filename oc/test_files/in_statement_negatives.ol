/**
 * Author: Jack Robbins
 * Test the use of an in statement with negative values
 */

pub fn in_with_negatives(x:i32) -> i32 {
	if(x in (-5, -11, 2, 0, 1)){
		ret 6;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @in_with_negatives(-11);
}
