/**
* Author: Jack Robbins
* Test the invalid case where we're trying to create a mutable reference to 
* an immutable value
*/

//Mutable reference params
pub fn add_refs(x:mut i32&, y:mut i32&) -> i32 {
	x++;
	y--;
	ret x + y;
}

pub fn main() -> i32 {
	let x:i32 = 3;
	let y:i32 = 3;

	//Should fail, attempting to acquire a mutable reference
	//to an immutable variable
	ret @add_refs(x, y);
}

