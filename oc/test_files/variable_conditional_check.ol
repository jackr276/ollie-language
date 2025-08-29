/**
* Author: Jack Robbins
* This file checks the case where you have a constant as a conditional: i.e while(1) or something of the sort
*/

pub fn main() -> i32 {
	let mut x:i32 := 232;
	let mut y:i32 := 3;

	//Checking this
	while(y){
		x--;

		break when (x == 32);
	}

	//To stop the optimizer
	ret x;
}
