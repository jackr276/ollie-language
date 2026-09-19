/**
 * Author: Jack Robbins
 * Test a valid case where the only difference is mutability in an overload type list, that being with a pointer
 */



pub fn pointer_add(x:i32*, y:i32) -> i32 {
	ret *x + y;
}


pub fn pointer_add(x:mut i32*, y:i32) -> i32 {
	*x += y + 2;
	ret *x;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 6;
	let x_ptr:mut i32* = &x;

	//Function pointerize it for testing
	let fn_ptr:fn(mut i32*, i32) -> i32 = pointer_add;
	
	OUNIT: [exit_status = 13]
	ret @fn_ptr(x_ptr, y);
}

