/**
* Author: Jack Robbins
* Test nested struct access
*/

define struct my_struct{
	next:struct my_struct*;
	ch:mut char;
	y:mut i32[3];
	lch:mut char;
} as custom_struct;


pub fn get_next_val(str:struct my_struct*) -> i32 {
	ret str=>next=>ch;
}

pub fn main() -> i32 {
	ret 0;
}
