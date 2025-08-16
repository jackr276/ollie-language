/**
* Author: Jack Robbins
* This program is made to test returning in an ollie style switch
*/

fn return_ollie_switch(arg:i32) -> i32{
	switch(arg){
		case 2 -> {
			ret 32;
		}
		case 1 -> {
			ret 2;
		}
		case 4 -> {
			ret 3;
		}
		case 3 -> {
			ret 5;
		}

		case 6 -> {
			ret 22;
		}

		default -> {
			ret 111;
		}
	}

	//Shouldn't even touch this
	ret 22;
}

pub fn main(arg:i32, argv:char**) -> i32{
	ret @return_ollie_switch(arg);
}
