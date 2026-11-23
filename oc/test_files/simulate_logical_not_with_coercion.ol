/**
 * Author: Jack Robbins
 * Test our ability to simulate logical not after coercion has occurred
 *
 * NOTE: due to a casting bug this does not work right yet
 */


pub fn logical_not_with_coercion() -> i32 {
	let x:f32 = 0.000000001;

	if(!(<i32>x)) {
		ret 11;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	OUNIT:[exit_status = 5]
	ret @logical_not_with_coercion();
}
