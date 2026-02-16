/**
* Author: Jack Robbins
* Simple testing for the address of a struct
*/

define struct my_struct {
	x:mut i32;
	y:mut i64;
}; 

pub fn mut_struct(u:mut struct my_struct*) -> void {
	u=>x = 33;
}

pub fn struct_mutatation() -> i32 {
	declare x:mut struct my_struct;

	let y:mut struct my_struct* = &x;
	
	@mut_struct(y);

	ret y=>x;
}

pub fn main() -> i32 {
	ret 0;
}
