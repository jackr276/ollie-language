/**
* Author: Jack Robbins
* Testing empty if statements
*/


fn empty_else(x:mut i32) -> i32 {
	if(x < 3){
		x--;
	} else if (x < 4){
		x++;
	} else {

	}

	ret x;
}


fn empty_else_if(x:mut i32) -> i32 {
	if(x < 3){
		x--;
	} else if (x < 4){
		//Empty on purpose
	} else {
		x++;
	}

	ret x;
}


fn empty_if(x:mut i32) -> i32 {
	if(x < 3){
		//Empty on purpose
	} else if (x < 4){
		x--;
	} else {
		x++;
	}

	ret x;
}


//Dummy main
pub fn main() -> i32 {
	ret	0;
}

