/**
* Author: Jack Robbins
* This file will test the ability of the compiler to recognize/reuse local constant
* values
*/

//Dummy string length function
pub fn string_length(str:mut char*) -> i32 {
	let x:mut i32 = 0;

	for(let i:mut i32 = 0; ;i++){
		//Means we hit the end
		if(str[i] == '\0'){
			break;
		}

		//Otherwise bump the index
		x++;
	}
}


pub fn main() -> i32 {
	//The compiler should recognize that "Hello"
	//is a complete duplicate and we should only see
	//one ".LC" value in the final assembly
	let x:i32 = @string_length(<mut char*>("Hello"));
	let y:i32 = @string_length(<mut char*>("Hello"));

	ret x + y;
}

