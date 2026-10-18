/**
* Author: Jack Robbins
* Test a case where we are copying from a dereferenced memory
* region
*/

//Define a struct type
define struct custom {
		x:mut i32;
		a:mut i64;
		y:mut char;
} as my_struct;


pub fn main() -> i32 {
	let x:my_struct = {2, 8, 'a'};
	
	//Get a pointer to it
	let ptr:my_struct* = &x;

	//Copy assignment from a deref
	let y:mut my_struct = *ptr;

	OUNIT: [exit_status = 'a']
	ret y:y;
}
