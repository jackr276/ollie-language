/**
* Author: Jack Robbins
* Test the case where we try to define a predeclared function with a mismatched error list
*/


define error custom_error1;
define error custom_error2;
define error custom_error3;


declare pub fn! my_func() -> i32 raises(custom_error1, custom_error2, custom_error3);


pub fn! my_func() -> i32 {
	ret 0;
}


pub fn main() -> i32 {
	ret 0;
}
