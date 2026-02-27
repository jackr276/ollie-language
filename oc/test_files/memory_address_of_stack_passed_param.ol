/**
* Author: Jack Robbins
* Test taking the memory address of a stack passed parameter
*/


pub fn modify_int(x:mut i32*) -> i32 {
	*x = 5;
}


pub fn more_than_6(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:mut i32) -> i32 {
	//Take the address of a stack passed parameter - this should be easy because it's already
	// on the stack
	@modify_int(&dd);
	
	ret x + y + z * aa - bb + cc - dd;
}


pub fn main() -> i32 {
	ret @more_than_6(1, 2, 3, 4, 5, 6, 7);
}
