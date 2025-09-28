/**
* Author: Jack Robbins
* Test the usage of the address operator on struct members
*/


fn mutate_int(mut x:i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare mut x_arr:i32[125];

	@mutate_int(&(x_arr[3]));

	ret 0;
}
