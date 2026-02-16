/**
* Author: Jack Robbins
* Test the ability of Ollie to access pointers inside of arrays
*/

pub fn pointer_array_access(x:i32) -> i32 {
	let pointer_array:char*[] = ["Hi", "hello", "how are you"];

	//This is going to require an intermediary dereference
	ret pointer_array[x][2];
}


pub fn main() -> i32 {
	ret 0;
}
