/**
* Author: Jack Robbins
* Test assigning a char array pointer
*/

pub fn assign_char_array_pointer(x:mut char**) -> void {
	x[2][3] = 'a';
}

pub fn main() -> i32 {
	ret 0;
}
