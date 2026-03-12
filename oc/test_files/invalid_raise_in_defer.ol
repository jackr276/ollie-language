/**
* Author: Jack Robbins
* Test an attempt to raise an error inside of a defer statement. This is not allowed
* as defers are guaranteed to execute a return right after they run, so this would be an 
* issue
*/

define error error1;


//Should fail - cannot raise an error in a defer
pub fn! error_in_defer(x:mut i32) -> i32 {
	defer{
		x++;
		raise error1;
	}

	ret x--;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
