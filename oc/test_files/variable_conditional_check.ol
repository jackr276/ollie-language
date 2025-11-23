/**
* Author: Jack Robbins
* This file checks the case where you have a constant as a conditional: i.e while(1) or something of the sort
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = 232;

	//Checking while
	while(*argv){
		x--;

		break when (x);
	}


	//Checking do-while
	do{
		x--;

		break when (x == 32);
	} while(argc);


	//Check that it works for if
	if(*argv){
		x--;
	}

	//Check that it works for else if
	if(argc){
		x++;
	} else if (**argv){
		x--;
	}

	//To stop the optimizer
	ret x;
}
