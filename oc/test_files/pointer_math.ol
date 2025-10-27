/**
* Author: Jack Robbins
* Test pointer arithmetic on the write side
*/


fn mutate_int(mut x:i32*) -> void {
	*(x + 1) = 2;
}

fn mutate_int2(mut x:i32*) -> void {
	*(x - 1) = 2;
}

pub fn main() -> i32 {
	declare mut x_arr:i32[125];

	@mutate_int(&(x_arr[3]));

	ret 0;
}
