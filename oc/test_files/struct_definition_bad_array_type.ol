/**
* Author Jack Robbins
* Bad recursive struct member defintion
*/

define struct my_struct {
	//Bad array definition
	x:mut struct my_struct[3];
	y:mut i32;
} as custOm_struct;


pub fn main() -> i32 {
	ret 0;
}
