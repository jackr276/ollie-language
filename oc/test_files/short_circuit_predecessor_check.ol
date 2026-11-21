/**
* Author: Jack Robbins
* Investigating a predecessor issue for short circuit
*/


pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut u32 = argc;
	
	if(x || argc){
		x = x - 3;
	} else {
	 	x = x + 3;
		ret x;
	}

	//So it isn't optimized away
	ret x;
}
