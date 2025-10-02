/**
* Author Jack Robbins
* Bad recursive struct member defintion
*/

define struct my_struct {
	//Bad recursive definition like so
	mut x:struct my_struct;
	mut y:i32;
} as custOm_struct;


pub fn main() -> i32 {
	ret 0;
}
