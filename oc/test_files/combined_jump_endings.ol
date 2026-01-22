/**
* Author: Jack Robbins
* This file will test the combined jump endings that have been problematic
* for ollie codegen in the past. Specifically things like an if with a breka
* inside of it
*/

pub fn string_length_for(str:char*) -> i32 {
	let x:mut i32 = 0;

	for(let i:mut i32 = 0;; i++, x++){
		if(str[i] == '\0'){
			break;
		}
	}

	ret x;
}

pub fn string_length_while(str:char*) -> i32 {
	let x:mut i32 = 0;
	let i:mut i32 = 0;

	while(true){
		if(str[i] == '\0'){
			break;
		}

		x++, i++;
	}

	ret x;
}


pub fn string_length_do_while(str:char*) -> i32 {
	let x:mut i32 = 0;
	let i:mut i32 = 0;

	do {
		if(str[i] == '\0'){
			break;
		}

		x++, i++;

	} while(true);

	ret x;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
