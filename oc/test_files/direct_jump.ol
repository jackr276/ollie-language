/**
* Author: Jack Robbins
* This file tests the direct jump functionality that ollie provides
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 := argc;
	let mut y:i32 := 2322;

	//So long as x is more than 0
	while(1){
		jump end_label;
		--x;
		y--;
	}


	//Basically a break statement, but we're just testing
	#end_label:

	//To avoid the optimization issues
	ret y;
}
