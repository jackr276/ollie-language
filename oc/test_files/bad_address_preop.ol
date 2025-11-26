/**
* Author: Jack Robbins
* Test the usage of the address operator on struct members
*/


define struct my_struct {
	x:mut i64;
	y:mut i32;
	c:mut char;
} as custom_struct;


fn mutate_int(x:mut i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare construct:mut custom_struct;

	//Should fail
	@mutate_int(&(++construct:y));

	ret 0;
}
