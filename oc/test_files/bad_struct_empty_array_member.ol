/**
* Author: Jack Robbins
* Test the definition of a struct type with an invalid empty array member
*/


//BAD struct
define struct bad_struct {
	x:mut i32;
	y:mut i32[]; //BAD
	z:mut char;
} as invalid;


pub fn main() -> i32{
	ret 0;
}
