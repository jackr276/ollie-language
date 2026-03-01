/**
* Author: Jack Robbins
* Test the very basic case where a function has more than 6 parameters
*/


pub fn more_than_6(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:mut i32) -> i32 {
	//Let's test storing to a stack param
	if(cc - bb == 0){
		dd = 5;
	} else {
		dd = 6;
	}

	ret x + y + z * aa - bb + cc - dd;
}



pub fn main() -> i32 {
	ret @more_than_6(1, 2, 3, 4, 5, 6, 7);
}
