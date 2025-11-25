/**
* Author: Jack Robbins
* This file tests the conditional jump(jump when) functionality that ollie provides
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = argc;
	let y:mut i32 = 2322;

	//So long as x is more than 0
	while(1){
		jump end_label when(x == 17);
		--x;
		y--;
	}


	//Basically a break statement, but we're just testing
	#end_label:

	//To avoid the optimization issues
	ret y;
}
