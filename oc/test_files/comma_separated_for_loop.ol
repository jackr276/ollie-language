/**
* Author: Jack Robbins
* Test the ability of comma separated statements to be used in the for block
*/

fn do_something(x:i32, y:i32) -> i32 {
	ret x * y - 3;
}


pub fn main() -> i32 {
	let counter:mut i32 = 0;

	for(let x:mut i32 = 3, let y:mut i32 = 5;
		x < 15 && y > -5;
		x++, y--){
		counter += @do_something(x, y);
	}

	ret counter;
}
