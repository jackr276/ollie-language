/**
 * Author: Jack Robbins
 * Test the use of a global function pointer variable
 */

declare fn_ptr:mut fn(i32, i32) -> i32;


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


//Set the function pointer
pub fn set_ptr() -> void {
	fn_ptr = add;
}


//Call it
pub fn call_ptr(x:i32, y:i32) -> i32 {
	ret @fn_ptr(x, y);
}

pub fn main() -> i32 {
	let x:i32 = 15;
	let y:i32 = 37;

	@set_ptr();
	OUNIT: [exit_status = 52]
	ret @call_ptr(x, y);
}
