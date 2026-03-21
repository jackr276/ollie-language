/**
* Author: Jack Robbins
* Test an invalid case where we try and assign a function with no elaborative param to a function pointer
* variable that does have it
*/

//Dummy - params not used but it should get the point across
pub fn elaborative(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn assign_elaborative() -> i32 {
	//Invalid assignment should not work
	let my_func:fn(i32, params i32) -> i32 = elaborative;

	ret @my_func(2, 5);
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
