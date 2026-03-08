/**
* Author: Jack Robbins
* Test a function pointer type that takes in no params
*/

define fn() -> i32 as custom_pointer;

pub fn blank() -> i32 {
	ret 0;
} 


pub fn main() -> i32 {
	let x:custom_pointer = blank;

	ret @x();
}
