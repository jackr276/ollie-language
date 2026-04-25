/**
* Author: Jack Robbins
* Super basic while tester
*/

pub fn while_short_circuit_and() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 6;
	let iter:mut i32 = 0;

	while((x > 0) && (y > 0)){
		x--;
		y--;
		iter++;
	}

	ret iter;
}

pub fn while_short_circuit_or() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 6;
	let iter:mut i32 = 0;

	while((x > 0) || (y > 0)){
		x--;
		y--;
		iter++;
	}

	ret iter;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let iter:mut i32 = 0;

	while(x > 0) {
		x--;
		iter++;
	}

	ret iter;
}
