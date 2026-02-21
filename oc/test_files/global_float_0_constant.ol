/**
* Author: Jack Robbins
* Test a global floating point constant that is set to be 0. Unlike
* the local floating point constants, we cannot use any fancy PXOR clearing
* tricks on this one
*/

let x:f32 = 0.0;

pub fn main() -> i32 {
	ret x;
}
