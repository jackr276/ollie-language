/**
 * Author: Jack Robbins
 * Test an in statement that is specifically not eligible to be lowered into a switch statement
 */


pub fn ineligible_in(x:f32) -> i32 {
	ret x in (2.22, 3.33, 4.44, 5.55, 6.2, 4.2, 1, 22.3);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret @ineligible_in(1);
}
