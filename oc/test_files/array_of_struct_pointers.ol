/**
* Author: Jack Robbins
* Test an array of struct pointers, and accessing them
*/

define struct my_struct{
	ch:mut char;
	lch:mut char;
	y:mut i32;

} as my_struct;


pub fn do_something_with_struct(x:mut my_struct*) -> void {
	x=>y = 2;
	x=>lch = 'a';
}

pub fn struct_mutator(arr:mut my_struct**) -> void {
	@do_something_with_struct(arr[2]);
}

pub fn main() -> i32 {
	ret 0;
}

