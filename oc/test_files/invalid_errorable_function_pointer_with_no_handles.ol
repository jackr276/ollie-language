/**
* Author: Jack Robbins
* Test the case where we try to call an errorable function pointer and forget the handles
*/

define error divide_by_zero_error_t;

pub fn! errorable_function(x:i32) -> i32 raises(divide_by_zero_error_t){
	if(x == 0) {
		raise divide_by_zero_error_t;
	}
	
	ret 2 / x;
}


pub fn main() -> i32 {
	//Make it a function pointer
	let x:fn!(i32) -> i32 raises(divide_by_zero_error_t) = errorable_function;

	//Going to fail - we need to see a handle statement here
	let result:i32 = @x(3);

	ret result;
}
