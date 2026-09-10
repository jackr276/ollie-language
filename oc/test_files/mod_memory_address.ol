/**
 * Author: Jack Robbins
 * Test a case where we do a weird operation(in this case mod) on a memory address
 */




pub fn main() -> i32 {
	let x:i32[5] = [1, 2, 3, 4, 5];
	let y:i32 = 5;
	declare result:i32;

	if(<i64>(&y) % 3 == 0) {
		result = 5;
	} else {
		result = 3;
	}
	

	OUNIT: [exit_status = 3]
	ret result;
}
