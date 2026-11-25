/**
 * Author: Jack Robbins
 * Test to see if our test not zero command can handle casting
 */


pub fn branch_on_cast() -> i32 {
	let static x:f32 = 0.0000001;

	if(<i32>x) {
		ret 5;
	} else {
		ret 11;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 11]
	ret @branch_on_cast();
}
