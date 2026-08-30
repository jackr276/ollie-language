/**
 * Author: Jack Robbins
 * Test our ability to postincrement and postdecrement a global variable
 */


let pub global:mut i32 = 5;


pub fn inc_and_get_global() -> i32 {
	global++;

	ret global;
}


pub fn dec_and_get_global() -> i32 {
	global--;

	ret global;
}


pub fn main() -> i32 {
	//Should return 6 + 5 = 11
	OUNIT: [exit_status = 11]
	ret @inc_and_get_global() + @dec_and_get_global();
}
