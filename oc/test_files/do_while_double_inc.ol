/**
* Author: Jack Robbins
* Handling an empty while loop
*/

pub fn main() -> i32 {
	let mut x:i32 = 0;
	let mut y:i32 = 1777;
	
	//Empty do-while
	do{}while(x++ < y--);

	ret x;
}
