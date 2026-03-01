/**
* Author: Jack Robbins
* Test passing by reference with a stack param
*/


pub fn more_than_6(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:mut i32*) -> i32 {
	//Let's test storing to a stack param
	if(cc - bb == 0){
		*dd = 5;
	} else {
		*dd = 6;
	}

	ret x + y + z * aa - bb + cc - *dd;
}



pub fn main() -> i32 {
	let x:mut i32 = 4;
	
	@more_than_6(1, 2, 3, 4, 5, 6, &x);

	ret x;
}
