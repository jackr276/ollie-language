/**
* Author: Jack Robbins
* This program tests the functionality of two ternaries in an expression
*/

fn main() -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 4;
	let mut z:i32 := 5;
	let mut a:i32 := 6;

	let ret_val:i32 := (x == 2 ? x else y) * (y == 3 ? z else a);

	ret ret_val;
}
