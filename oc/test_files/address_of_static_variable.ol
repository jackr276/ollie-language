/**
* Author: Jack Robbins
* Test the compiler's handling of us taking the address of a static variable
*/

pub fn modify_pointer(x:mut i32*) -> void {
	*x = 5;
}


pub fn static_var(x:i32) -> i32 {
	let static my_var:mut i32 = 0;

	my_var += x;

	//Take the address of it here
	@modify_pointer(&my_var);

	ret my_var;
}


pub fn main() -> i32 {
	ret 0;
}
