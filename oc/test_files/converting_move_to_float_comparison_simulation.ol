/**
 * Author: Jack Robbins
 * Simulate comparison statements that require converting moves
 */


pub fn converting_move_not_equals() -> i32 {
	let x:i32 = 0;

	if(x != 0.0) {
		ret 5;
	} else {
		ret 7;
	}
}


pub fn float_int_compare() -> i32 {
	let x:f64 = 0.0 / 0.0;
	let y:i64 = -111;

	if(x == y) {
		ret 55;
	} else {
		ret 17;
	}
}


pub fn main() -> i32 {
	//Should return 7 + 17
	OUNIT: [exit_status = 24]
	ret @converting_move_not_equals()
		+ @float_int_compare();
}
