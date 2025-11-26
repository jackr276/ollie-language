/**
* Author: Jack Robbins
* Test the case where we attempt to modify an immutable field in a mutable struct
*/

define struct my_struct {
	x:mut i64;
	//Not mutable
	a:char;
} as custom_struct;


pub fn main() -> i32 {
	declare str:mut custom_struct;

	//This should fail
	str:a = 'a';

	ret 0;
}
