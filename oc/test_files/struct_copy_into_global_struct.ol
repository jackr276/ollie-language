/**
 * Author: Jack Robbins
 * Test our ability to copy into a global struct variable
 */

define struct my_struct {
	x:i32;
	arr:i32[10];
	y:f32;
} as custom_struct;

//Global struct var
declare global_struct:mut struct my_struct;


pub fn populate_global_struct(ptr:struct my_struct*) -> i32 {
	global_struct = *ptr;
}



pub fn main() -> i32 {
	let dummy:struct my_struct = {5, [20, 21, 22, 23, 24, 25, 26, 27, 28, 29], 'a'};

	@populate_global_struct(&dummy);

	OUNIT: [exit_status = 27]
	ret global_struct:arr[7];
}
