/**
 * Author: Jack Robbins
 * Test to see if our test not zero command can handle casting
 */


pub fn branch_on_cast(x:f32) -> i32 {
	if(<i32>x) {
		ret 5;
	} else {
		ret 11;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 11]
	ret @branch_on_cast(0.000001);
}
