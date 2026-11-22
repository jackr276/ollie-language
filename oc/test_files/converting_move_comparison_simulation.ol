/**
 * Author: Jack Robbins
 * Simulate comparison statements that require converting moves
 */


pub fn converting_move_not_equals() -> i32 {
	let x:f32 = 0.0 / 0.0;

	if(x != 3) {
		ret 5;
	} else {
		ret 7;
	}
}

pub fn signed_unsigned_coerce() -> i32 {
	let x:i64 = -1;
	let y:u64 = ~0;

	if(x == y) {
		ret 7;
	} else {
		ret 11;
	}
}


pub fn float_int_compare() -> i32 {
	let x:f64 = -111;
	let y:i64 = -111;

	if(x == y) {
		ret 55;
	} else {
		ret 17;
	}
}


pub fn main() -> i32 {
	//Should return 5 + 7 + 55
	OUNIT: [exit_status = 67]
	ret @converting_move_not_equals()
		+ @signed_unsigned_coerce()
		+ @float_int_compare();
}
