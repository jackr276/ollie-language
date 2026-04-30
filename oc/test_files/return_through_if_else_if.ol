/**
* Author: Jack Robbins
* Test our ability to return through every path in an if-else-if with no else clause
*/

pub fn return_through_every_path(x:i32) -> i32 {
	let result:mut i32 = 5;

	if(x == 3){
		ret 5;
	} else if(x == 2){
		ret 2;
	} else if(x == 1){
		ret 1;
	}

	ret result;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
