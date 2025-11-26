/**
* Author: Jack Robbins
* Test the address operator on more complex memory structures
*/


define struct my_struct {
	x:mut i64;
	y:mut i32[323];
	c:mut char;
} as custom_struct;


fn mutate_int(x:mut i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare construct:custom_struct;

	@mutate_int(&(construct:y[233]));

	ret 0;
}
