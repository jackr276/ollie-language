/**
* Author: Jack Robbins
* Test a function pointer type that takes in no params
*/

pub fn blank() -> i32 {
	ret 0;
} 


pub fn main() -> i32 {
	let x:fn() -> i32 = blank;

	ret @x();
}
