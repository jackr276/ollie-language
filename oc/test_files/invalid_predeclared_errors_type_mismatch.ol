/**
* Author: Jack Robbins
* Test a case where we have a mismatch
*/

define error error1;
define error error2;
define error error3;

declare pub fn! too_long(i32) -> i32 raises (error1, error2);


pub fn! too_long(x:i32) -> i32 raises (error3, error2){
	ret 0;
}


pub fn main() -> i32 {
	ret 0;
}
