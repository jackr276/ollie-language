/**
* Author: Jack Robbins
* Test the definition of a struct type with an invalid empty array member
*/


//BAD struct
define struct bad_struct {
	mut x:i32;
	mut y:i32[]; //BAD
	mut z:char;
} as invalid;


pub fn main() -> i32{
	ret 0;
}
