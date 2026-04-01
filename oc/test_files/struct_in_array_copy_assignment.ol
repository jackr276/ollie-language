/**
* Author: Jack Robbins
* Test our ability to invoke a copy assignment when we're copying from a struct to an array of structs
*/


define struct my_struct {
	x:i32;
	y:i64;
	z:char;
} as custom_struct;


pub fn main() -> i32 {
	declare struct_array:mut custom_struct[10];
	let to_be_copied:custom_struct = {1, 2, 'a'};

	//Now the actual copying part
	struct_array[3] = to_be_copied;

	//Should return 1 if this worked
	ret struct_array[3]:x;
}
