/**
* Author: Jack Robbins
* Validate that our function parameter matching is correct if we're declaring
* a predeclared function with an elaborative param and then trying to 
* actually declare it without one
*/

declare fn my_fn(i32, params i32) -> i32;

//Should fail
fn my_fn(x:i32, y:i32) -> i32 {
	ret x + params[1] + params[2];
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
