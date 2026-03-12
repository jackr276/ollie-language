/**
* Author: Jack Robbins
* Test the use of a duplicate error type inside of a function prototype(error) declaration
*/

define error error1;
define error error2;
define error error3;


//This is bad - we've got duplicates
pub fn dynamic_dispatch(
						//Remember we're allowed to do something like this
						x:fn!(i32, i32) -> i32 raises (error1, error2, error2)
						) -> {
	ret @x(2, 3);
	
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
