/**
* Author: Jack Robbins
* Handling an empty while loop
*/

pub fn main() -> i32 {
	let x:mut i32 = 0;
	
	//Empty do-while
	do{}while(x++ < 5);

	ret x;
}
