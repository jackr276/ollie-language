/**
* Author: Jack Robbins
* Test the address operator on more complex memory structures
*/


define struct my_struct {
	mut x:i64;
	mut y:i32[323];
	mut c:char;
} as custom_struct;


fn mutate_int(mut x:i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare mut construct:custom_struct;

	@mutate_int(&(construct:y[233]));

	ret 0;
}
