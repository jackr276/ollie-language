/**
* Author: Jack Robbins
* Super basic do while tester
*/


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let iter:mut i32 = 0;

	while(x > 0){
		x--;
		iter++;
	}

	ret iter;
}
