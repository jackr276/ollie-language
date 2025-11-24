/**
* Author: Jack Robbins
* Test the address operator on more complex memory structures
*/

define struct internal_struct {
	x:mut i64;
	c:mut i32;
} as internal_struct_type;


define struct my_struct {
	x:mut i64;
	//Gigantic array
	y:mut internal_struct_type[323];
	c:mut char;
} as custom_struct;


fn mutate_int(x:mut i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	declare construct:mut custom_struct;

	@mutate_int(&(construct:y[233]:c));

	ret 0;
}
