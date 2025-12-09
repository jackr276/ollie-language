/**
* Author: Jack Robbins
* Test the case where we want to post-increment a reference
*/

fn mutate_x(x_ref:mut i32*) -> i32 {
	ret *x_ref + 1;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 5;

	//This should now be forced into the stack
	let x_ref:mut i32* = &x;

	//Trigger the postinc
	x++;

	ret @mutate_x(x_ref);
}
