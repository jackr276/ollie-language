/**
 * Author: Jack Robbins
 * Test a case where we do a weird operation(in this case mod) on a memory address
 */


pub fn main() -> i32 {
	let x:i32[5] = [1, 2, 3, 4, 5];
	let y:i32 = 5;
	declare result:i32;

	let mod_val:i32 = 2;

	//This should never actually work we shouldn't
	//have addresses that are odd
	if(<i64>(&y) % mod_val != 0) {
		result = 5;
	} else {
		result = 3;
	}

	OUNIT: [exit_status = 3]
	ret result;
}
