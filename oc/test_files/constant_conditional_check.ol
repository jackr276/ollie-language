/**
* Author: Jack Robbins
* This file checks the case where you have a constant as a conditional: i.e while(1) or something of the sort
*/

pub fn main() -> i32 {
	let mut x:i32 := 232;

	//Checking while
	while(1){
		x--;

		break when (x == 32);
	}


	//Checking do-while
	do{
		x--;

		break when (x == 32);
	} while(1);

	//checking for
	for(let mut a:i32 := 32; a; a--){
		x--;
	}

	//Check that it works for if
	if(1){
		x--;
	}

	//Check that it works for else if
	if(x == 1){
		x++;
	} else if (1){
		x--;
	}

	//To stop the optimizer
	ret x;
}
