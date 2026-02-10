/**
* Author: Jack Robbins
* Test logical not being always true/false
*/


pub fn logical_not_always_true(x:i32) -> i32 {
	//Always true
	if(!false) {
		ret x + 1;
	} else {
		ret x - 3;
	}
}


pub fn logical_not_always_false(x:i32) -> i32 {
	//Always false
	if(!5) {
		ret x + 1;
	} else {
		ret x - 3;
	}
}

pub fn main() -> i32 {	
	ret 0;
}
