/**
* Author: Jack Robbins
* Test the case where we are returning a reference to a variable
* from a function. We are using the technically invalid case 
* of returning a reference to a local variable, but this only 
* exists to test the underlying compiler machinery so it's fine
*/


fn get_max(a:i32&, b:i32&) -> i32& {
	ret (a > b) ? a else b;
}

pub fn main() -> i32 {
	let x:mut i32 = 10;
	let y:mut i32 = 15;

	//The compiler should implicitly get the addresses of x and y here
	@get_max(x, y);

	//We should know that since we're returning
	//an i32, but we have a reference, that we want to dereference
	ret 0;
}
