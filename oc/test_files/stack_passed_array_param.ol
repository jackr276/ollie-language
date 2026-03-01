/**
* Author: Jack Robbins
* Test the ability of the system to handle a stack passed array param - both stores and loads
*/


pub fn array_as_stack_param(x:i32, y:i32, z:i32, a:char, b:char, c:char, arr:mut i32[5]) -> i32 {
	if(x - a + b + c == 0){
		arr[1] = 5;
	} else {
		arr[2] = 5;
	}

	ret x + y + z + arr[a];
}


pub fn main() -> i32 {
	ret 0;
}
