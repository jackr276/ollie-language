/**
* Author: Jack Robbins
* Testing for an infinite do while with the virtual exit
*/

pub fn main() -> i32 {
	let x:mut i32 = 232;

	//Checking do-while
	do{
		x--;

		break when (x == 32);
	} while(1);

	ret x;
}
