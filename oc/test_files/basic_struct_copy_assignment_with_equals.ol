/**
* Author: Jack Robbins
* Test struct copying when we have an assignment operation as opposed to a
* let statement
*/

//Define a struct type
define struct custom {
		x:mut i32;
		a:mut i64;
		y:mut char;
} as my_struct;


pub fn main() -> i32 {
	declare y:mut my_struct;
	let x:my_struct = {2, 8, 'a'};

	//Copy assignment here
	y = x;

	ret y:y;
}
