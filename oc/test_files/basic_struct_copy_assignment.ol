/**
* Author: Jack Robbins
* Test the most basic case where we're assigning from one local struct over to another
*/

//Define a struct type
define struct custom {
		x:mut i32;
		a:mut i64;
		y:mut char;
} as my_struct;


pub fn main() -> i32 {
	let x:my_struct = {2, 8, 'a'};

	//Copy assignment here
	let y:mut my_struct = x;

	ret y:2;
}
