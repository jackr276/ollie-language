/**
* Author: Jack Robbins
* Validate that we are able to assign a valid function pointer to a function
* with an elaborative param
*/

//Should work just fine
fn my_fn(x:i32, y:params i32) -> i32 {
	ret x + y[1] + y[2];
}


pub fn main() -> i32 {
	//Use a function pointer to assign
	let my_func:fn(i32, params i32) -> i32 = my_fn;

	ret @my_func(2, 3, 4, 5, 6);
}
