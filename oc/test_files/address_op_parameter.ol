/**
* Author: Jack Robbins
* Testing taking the address of a parameter
*/

fn address_op_parameter(param:mut i32) -> i32{
	//Address of a parameter
	let x:mut i32* = &param;

	//Do some stuff
	*x = 32;
	*x = 17;

	//Give it back
	ret *x;
}


pub fn main() -> i32 {
	ret @address_op_parameter(27);
}
