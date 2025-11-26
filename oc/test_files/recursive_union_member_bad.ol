/**
* Author Jack Robbins
* Bad recursive union member defintion
*/

define union my_union {
	//Bad recursive definition like so
	x:union my_union;
	y:i32;
} as custom_union;

pub fn main() -> i32 {
	ret 0;
}
