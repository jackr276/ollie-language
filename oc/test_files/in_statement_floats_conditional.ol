/**
 * Author: Jack Robbins
 * Test an in statement that is specifically not eligible to be lowered into a switch statement
 * in the context of a conditional
 */

pub fn ineligible_in(x:f32) -> i32 {
	if(x in (2.22, 3.33, 4.44, 5.55, 6.2, 4.2, 1, 22.3)){
		ret 55;
	} else {
		ret 66;
	}
} 


pub fn main() -> i32 {
	OUNIT: [console = 55]
	ret @ineligible_in(3.33);
}
