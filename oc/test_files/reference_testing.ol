/**
* Author: Jack Robbins
* Full test of all reference features that we currently support
*/

//Return references
pub fn return_reference(x_ref:mut i32&, y_ref:mut i32&) -> i32& {
	x_ref += 1;
	x_ref -= 1;
	y_ref += 1;
	y_ref *= 33;
	
	//Return a reference
	ret x_ref;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 4;

	let x_ref:mut i32& = x;
	let y_ref:mut i32& = y;

	x--;
	--y;

	//Should implicitly dereference
	ret @return_reference(x_ref, y_ref);
}
