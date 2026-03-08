/**
* Author: Jack Robbins
* A very basic test for Ollie errors with just one function
*/


pub fn! ollie_error(x:i32, y:i32) -> i32|{artihmetic_error} {
	if(y == 0){
		raise arithmetic_error;
	}

	ret x / y;
} 


pub fn main() -> i32 {
	let result:i32 = @ollie_error(x, y) -> {}
}
