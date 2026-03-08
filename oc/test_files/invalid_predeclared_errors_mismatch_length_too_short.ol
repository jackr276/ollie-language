/**
* Author: Jack Robbins
* Test a case where we have too few predeclared errors
*/

define error error1;
define error error2;
define error error3;

declare pub fn! too_long(i32) -> i32 raises (error1, error2, error3);


pub fn! too_long(x:i32) -> i32 raises (error1, error2){
	ret 0;
}


pub fn main() -> i32 {
	ret 0;
}
