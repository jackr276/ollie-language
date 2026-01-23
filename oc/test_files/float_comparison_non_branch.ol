/**
* Author: Jack Robbins
* Test float comparison for non-branching instructions. This will invoke the cmpss/cmpsd subsystem
* to bypass ucomiss/ucomisd altogether
*/

pub fn float_lt(x:f32, y:f32) -> i32 {
	ret x < y;
}

pub fn double_lt(x:f64, y:f64) -> i32 {
	ret x < y;
}

pub fn float_le(x:f32, y:f32) -> i32 {
	ret x <= y;
}

pub fn double_le(x:f64, y:f64) -> i32 {
	ret x <= y;
}
 
pub fn float_gt(x:f32, y:f32) -> i32 {
	ret x > y;
}

pub fn double_gt(x:f64, y:f64) -> i32 {
	ret x > y;
}

pub fn float_ge(x:f32, y:f32) -> i32 {
	ret x >= y;
}

pub fn double_ge(x:f64, y:f64) -> i32 {
	ret x >= y;
}

pub fn float_e(x:f32, y:f32) -> i32 {
	ret x == y;
}

pub fn double_e(x:f64, y:f64) -> i32 {
	ret x == y;
}

pub fn float_ne(x:f32, y:f32) -> i32 {
	ret x != y;
}

pub fn double_ne(x:f64, y:f64) -> i32 {
	ret x != y;
}

pub fn main() -> i32 {
	ret 0;
}
