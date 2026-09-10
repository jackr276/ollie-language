/**
 * Author: Jack Robbins
 * Test a case where we do a weird operation(in this case mod) on a stack parameter memory address
 */

define struct my_struct {
	x:i64;
	y:i64;
	z:i32[11];
};


/**
 * Yes this is a very contrived example, I just want to illustrate
 * that we need this to work
 */
pub fn is_addr_even(input:struct my_struct) -> bool {
	//LSB is 1 means that we're odd
	let mask:i32 = 0x01;

	if(<i64>(&(input)) & mask == 0) {
		ret true;
	} else {
		ret false;
	}
}


pub fn main() -> i32 {
	let passer:struct my_struct = {5, 5, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]};

	OUNIT: [exit_status = true]
	ret @is_addr_even(passer);
}
