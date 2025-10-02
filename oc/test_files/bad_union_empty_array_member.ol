/**
* Author: Jack Robbins
* Test the definition of a union type with an invalid empty array member
*/


//BAD struct
define union bad_union {
	mut x:i32;
	mut y:i32[][3]; //BAD
	mut z:char;
} as invalid;


pub fn main() -> i32{
	ret 0;
}
