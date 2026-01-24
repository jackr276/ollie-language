/**
* Author: Jack Robbins
* Testing for floating point branching
*/

pub fn float_compare_e(x:f32, y:f32) -> i32 {
	if(x == y){
		ret 0;
	} else {
		ret 1;
	}
}


pub fn float_compare_ne(x:f32, y:f32) -> i32 {
	if(x != y){
		ret 0;
	} else {
		ret 1;
	}
}


pub fn float_compare_g(x:f32, y:f32) -> i32 {
	if(x > y){
		ret 0;
	} else {
		ret 1;
	}
}


pub fn float_compare_ge(x:f32, y:f32) -> i32 {
	if(x >= y){
		ret 0;
	} else {
		ret 1;
	}
}


pub fn float_compare_l(x:f32, y:f32) -> i32 {
	if(x < y){
		ret 0;
	} else {
		ret 1;
	}
}


pub fn float_compare_le(x:f32, y:f32) -> i32 {
	if(x < y){
		ret 0;
	} else {
		ret 1;
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
