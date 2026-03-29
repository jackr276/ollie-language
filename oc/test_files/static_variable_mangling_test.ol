/**
* Author: Jack Robbins
* Test the compiler's ability to mangle static variable names if they match to avoid collisions
*/

pub fn static_var1(x:i32) -> i32 {
	let static my_var:i32 = 5;
	
	ret my_var + x;
}


pub fn static_var2(x:i32) -> i32 {
	let static my_var:i32 = 5;
	
	ret my_var + x;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
