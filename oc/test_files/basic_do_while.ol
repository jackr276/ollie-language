/**
* Author: Jack Robbins
* Super basic do while tester
*/

pub fn do_while_short_circuit() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 6;
	let iter:mut i32 = 0;

	do {
		x--;
		y--;
		iter++;
	} while((x > 0) && (y > 0));

	ret iter;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let iter:mut i32 = 0;

	do {
		x--;
		iter++;
	} while(x > 0);

	ret iter;
}
