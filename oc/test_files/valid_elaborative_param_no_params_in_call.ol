/**
* Author: Jack Robbins
* Test the case where we(validly) decide not to provide anything to an elaborative param
*/


pub fn elaborative_params(x:mut i32, y:params i32) -> i32 {
	for(let i:mut i32 = 0; i < paramcount(y); i++){
		x += y[i];
	}

	ret x;
}


pub fn main() -> i32 {
	//Valide use where we don't pass anything
	ret @elaborative_params(1);
}
