/**
* Author: Jack Robbins
* Test the nested compressed equality operation
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let result:mut i32 = 0;

	//Classic example of how someone would do this
	while((x += 5) != 5499){
		result++;
	}

	ret result;
}
