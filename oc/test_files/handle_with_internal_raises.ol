/**
* Author: Jack Robbins
* Test the case where we are raising errors inside of a handle statement itself. This will likely
* become a common use case if we want to propogate errors up the chain
*/

define error divide_by_zero_error_t;
define error arithmetic_error_t;

pub fn! divide_values(x:mut i32*, y:i32) -> void raises(divide_by_zero_error_t, arithmetic_error_t) {
	//Basic error case here
	if(*x == 0) {
		raise divide_by_zero_error_t;
	}

	if(y == 0){
		raise arithmetic_error_t;
	}

	*x = *x / y;
}

/**
* The hanlder will call this other function
*/
pub fn! handle_divide_values(x:mut i32, y:i32) -> i32 raises (divide_by_zero_error_t) {
	@divide_values(&x, y) handle(
								//This is the same as throwing an error up the chain
								divide_by_zero_error_t => raise divide_by_zero_error_t,
								arithmetic_error_t => ignore,
								error => ret -1
								);
	ret x;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:i32 = 0;

	//Effectively swallow by making these 0
	x = @handle_divide_values(x, y) handle (
											divide_by_zero_error_t => 0,
											error => 0
											);


	ret x;
}
