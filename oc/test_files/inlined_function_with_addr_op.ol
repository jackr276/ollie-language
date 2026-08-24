/**
 * Author: Jack Robbins
 * Test the use of an inlined function when we have the address operator(&) being
 * used
 */


inline fn use_addr_op(x:mut i32*) -> void {
	*x = 11;
}


inline fn inline_addr_op() -> i32 {
	declare x:mut i32;

	//Use the address op
	@use_addr_op(&x);

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 11]
	ret @inline_addr_op();
}
