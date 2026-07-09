/**
 * Author: Jack Robbins
 * Test an in statement that is specifically not eligible to be lowered into a switch statement
 */


pub fn ineligible_in(x:i32) -> i32 {
	ret x in (2, 3.33, 14, 5, 6, 4, 1, 22);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret @ineligible_in(1);
}
