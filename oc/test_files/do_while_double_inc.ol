/**
* Author: Jack Robbins
* Handling an empty while loop
*/

pub fn main() -> i32 {
	let x:mut i32 = 0;
	let y:mut i32 = 1777;
	
	//Empty do-while
	do{}while(x++ < y--);

	ret x;
}
