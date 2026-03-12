/**
* Author: Jack Robbins
* A very basic test for Ollie errors with just one function
*/

//We are able to define our own custom errors in Ollie. In reality
//these are just types that have special restrictions/a special purpose for us
define error arithmetic_error_t;
define error divide_by_zero_error_t;

/**
* To denote that a function can/may throw any error at all, 
* we use the ! after the "fn" keyword. It is possible for
* someone to use Ollie without using the error system at all
*/
pub fn! ollie_error(x:i32, y:i32) -> i32 raises(arithmetic_error_t, divide_by_zero_error_t) {
	if(y == 0){
		raise arithmetic_error_t;
	} 

	ret x / y;
} 


pub fn main() -> i32 {
	//Is kind of a pseudo-switch statement in a way
	//We make it so that you have no choice but to handle errors
	//This is a hidden switch statement under the hood


	//TODO ADD HANDLING
	let result:i32 = @ollie_error(3, 2);

	ret result;
}
