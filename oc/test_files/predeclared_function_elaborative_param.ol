/**
* Author: Jack Robbins
* Validate that our function parameter matching is correct if we're declaring
* a predeclared function with an elaborative param.
*/

declare fn my_fn(i32, params i32) -> i32;

//Should work just fine
fn my_fn(x:i32, y:params i32) -> i32 {
	ret x + y[1] + y[2];
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
