/**
* Author: Jack Robbins
* This file will test the ability of the compiler to recognize/reuse local constant
* values
*/

//Dummy string length function
pub fn string_length(str:char*) -> i32 {
	let x:i32 = 0;

	while(*str != '\0'){
		x++;
		str++;
	}
}


pub fn main() -> i32 {
	//The compiler should recognize that "Hello"
	//is a complete duplicate and we should only see
	//one ".LC" value in the final assembly
	let x:i32 = @string_length("Hello");
	let y:i32 = @string_length("Hello");

	ret x + y;
}

