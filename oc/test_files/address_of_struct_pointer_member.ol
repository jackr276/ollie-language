/**
* Author: Jack Robbins
* Test the usage of the address operator on struct members
*/


define struct my_struct {
	mut x:i64;
	mut y:i32;
	mut c:char;
} as custom_struct;


fn mutate_int(mut x:i32*) -> void {
	*x = 2;
}

fn mutate_struct_pointer_member(mut x:custom_struct*) -> void {
	@mutate_int(&x=>y);
}

pub fn main() -> i32 {
	declare mut construct:custom_struct;

	@mutate_struct_pointer_member(&construct);

	ret 0;
}
