/**
* Author: Jack Robbins
* Investigating a predecessor issue for short circuit
*/


pub fn main(arg:i32, argv:char**) -> i32{
	let x:mut u32 = 232;
	
	if(x || arg){
		x = x - 3;
	} else {
	 	x = x + 3;
		ret x;
	}

	//So it isn't optimized away
	ret x;
}
