/**
* Author: Jack Robbins
* Test the case where we've done branch reduction on an if statement
* We can do this branch reduction when we've found a pattern that says
* the conditional is always true
*/

pub fn if_condition() -> i32 {
	//Always true
	if(1){
		ret 0;
	} else {
		ret -1;
	}
}


pub fn main() -> i32 {
	ret @if_condition();
}

