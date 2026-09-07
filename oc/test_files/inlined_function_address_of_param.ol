/**
 * Author: Jack Robbins
 * Test the case of an inlined function where we take the address of a specifically
 * non stack-passed parameter
 */


pub fn modify_ptr(x:mut i32*) -> void {
	*x += 5;
}


pub inline fn inlined_address_of_param(x:mut i32, y:i32) -> i32 {
	//See how we handle this address being taken
	@modify_ptr(&x);
	
	ret x + y;
}


pub fn main() -> i32 {
	let y:mut i32 = 6;

	//Should make y into 11
	@modify_ptr(&y);

	//Should return 15 + 11 = 26
	OUNIT: [exit_status = 26]
	ret @inlined_address_of_param(10, 11);
}
