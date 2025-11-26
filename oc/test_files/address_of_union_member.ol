/**
* Author: Jack Robbins
* Test the usage of the address operator on struct members
*/


define union my_union {
	x:i64;
	y:i32;
	c:char;
} as custom_union;


fn mutate_int(x:mut i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare union_type:mut custom_union;

	@mutate_int(&(union_type.y));

	ret 0;
}
