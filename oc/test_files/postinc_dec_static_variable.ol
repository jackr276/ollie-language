/**
 * Author: Jack Robbins
 * Test our ability to postincrement and postdecrement a static variable
 */



pub fn inc_and_get_static() -> i32 {
	let static global:mut i32 = 18;

	global++;

	ret global;
}


pub fn dec_and_get_static() -> i32 {
	let static global:mut i32 = 11;

	global--;

	ret global;
}


pub fn main() -> i32 {
	//Should return 19 + 20 + 10 = 59
	OUNIT: [exit_status = 59]
	ret @inc_and_get_static() + @inc_and_get_static() + @dec_and_get_static();
}
