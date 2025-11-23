/**
* Author: Jack Robbins
* Test the usage of the address operator on struct members
*/


fn mutate_int(x:mut i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare x_arr:mut i32[125];

	@mutate_int(&(x_arr[3]));

	ret 0;
}
