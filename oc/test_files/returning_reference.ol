/**
* Author: Jack Robbins
* Test the case where we are returning a reference to a variable
* from a function. We are using the technically invalid case 
* of returning a reference to a local variable, but this only 
* exists to test the underlying compiler machinery so it's fine
*/

//This should return a reference - there should be no auto-deref
//happening here
fn get_max(a:i32&, b:i32&) -> i32& {
	ret (a > b) ? a else b;
}

pub fn main() -> i32 {
	let x:mut i32 = 10;
	let y:mut i32 = 15;

	//The compiler should implicitly get the addresses of x and y here
	ret @get_max(x, y);
}
