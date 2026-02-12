/**
* Author: Jack Robbins
* Test the always true optimizations for breaks in a switch
*/

pub fn always_true_switch(x:mut i32) -> i32 {
	switch(x){
		case 1:
			break when(true);
		case 2:
			x++;
			break;
		default:
			break;
	}
		
	ret x;
}

pub fn main() -> i32 {
	ret 0;
}
