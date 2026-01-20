/**
* Author: Jack Robbins
* Test nested assignment parsing
*/

//Dummy function
pub fn add_constant(x:i32) -> i32 {
	ret x + 2;
}


pub fn main() -> i32 {
	let x:mut i32 = 43;
	let result:mut i32 = 0;

	//Classic example of the way that this would work
	while((x = @add_constant(x)) < 1000){
		result++;
	}

	ret result;
}
