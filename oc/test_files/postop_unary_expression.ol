/**
 * Author: Jack Robbins
 * Test our ability to perform postincrement and postdecrement operations on the LHS using 
 * pointer values
 */



pub fn preinc_ptr(x:mut i32*) -> i32 {
	ret (*x)++;
}


pub fn predec_ptr(x:mut i32*) -> i32 {
	ret (*x)--;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;

	//X is now 6
	@preinc_ptr(&x);
	//X is now 7
	@preinc_ptr(&x);
	//X is not 6
	@predec_ptr(&x);

	OUNIT: [exit_status = 6]
	ret x;
}
