/**
 * Author: Jack Robbins
 * Simulate arithmetic that requires conversion
 */


pub fn expanding_int_subtract() -> i32 {
	let x:i64 = -1;
	let y:i16 = -1;

	if(x - y) {
		ret 55;
	} else {
		ret 17;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 17]
	ret @expanding_int_subtract();
}
