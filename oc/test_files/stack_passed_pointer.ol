/**
* Author: Jack Robbins
* Test the ability of the system to handle a stack passed array param - both stores and loads
*/

pub fn pointer_param(x:i32, y:i32, z:i32, a:char, b:char, c:char, ptr:mut i32*) -> i32 {
	if(x - a + b + c == 0){
		*ptr = 5;
	} else {
		*ptr = 5;
	}

	ret x + y + z + *ptr;
}


pub fn pointer_param_reassign(x:mut i32, y:i32, z:i32, a:char, b:char, c:char, ptr:mut i32*) -> i32 {
	//Now try to reassign it
	ptr = &x;

	//Modify it
	*ptr = 5;

	//Try a reassign
	let new_ptr:mut i32* = ptr;

	ret x + y + z + *new_ptr;
}



pub fn main() -> i32 {
	ret 0;
}
