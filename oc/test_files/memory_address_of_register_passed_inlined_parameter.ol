/**
 * Author: Jack Robbins
 * Verify that taking the memory address of a register passed parameter
 * inside of an inlined function works just fine
 */


pub fn modify_value(x:mut i32*) -> void {
	*x = 5;
}


inline fn inlined_update(x:mut i32) -> i32 {
	@modify_value(&x);

	ret x;
}


pub fn main() -> i32 {
	let x:i32 = 15;

	OUNIT: [exit_status = 5]
	ret @inlined_update(x);
}
