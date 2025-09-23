/**
* Author: Jack Robbins
* Test the way that the optimizer handles pre/post ops in unique scenarios
*/



//Should not optimize away
pub fn preincrement_return(mut a:i32) -> i32 {
	ret ++a;
}

//Should optimize away
pub fn postincrement_return(mut a:i32) -> i32 {
	ret a++;
}


pub fn main() -> i32 {
	ret 0;
}
