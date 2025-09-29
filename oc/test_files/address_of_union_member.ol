/**
* Author: Jack Robbins
* Test the usage of the address operator on struct members
*/


define union my_union {
	mut x:i64;
	mut y:i32;
	mut c:char;
} as custom_union;


fn mutate_int(mut x:i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare mut union_type:custom_union;

	@mutate_int(&(union_type.y));

	ret 0;
}
