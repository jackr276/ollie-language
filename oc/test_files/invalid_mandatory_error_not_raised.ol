/**
* Author: Jack Robbins
* Test the case where a function has a mandatory error but said error is never raised. This is an error case for
* us
*/

define error arithmetic_error_t;
define error divide_by_zero_error_t;

/**
* divide_by_zero_error_t is never actually thrown, this is a compiler error
*/
pub fn! ollie_error(x:i32, y:i32) -> i32 raises(arithmetic_error_t, divide_by_zero_error_t) {
	if(y == -1){
		raise arithmetic_error_t;
	} 
	
	ret x / y;
} 


pub fn main() -> i32 {
	/**
	* Is kind of a pseudo-switch statement in a way
	* We make it so that you have no choice but to handle errors
	* This is a hidden switch statement under the hood
	*/
	let result:i32 = @ollie_error(3, 2) handle( 
												arithmetic_error_t => ret -1,
												divide_by_zero_error_t => ret -1,
												//Assigns the return result value of the entire thing to -1
												error => -1
											   );

	ret result;
}
