/**
 * Author: Jack Robbins
 * Test a case where we use a ternary to decide which function to call inside of the call
 * statement itself
 */


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}


pub fn make_call_choice(x:i32, y:i32, choice:bool) -> i32 {
	//Make the call determination right in the ternary
	ret @(choice ? add else sub)(x, y);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 11]
	ret @make_call_choice(55, 44, false);
}
