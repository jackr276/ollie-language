/**
* Author: Jack Robbins
* Testing simple for loop functionality
*/

pub fn main() -> i32 {
	declare mut x:i32;
	let mut y:i32 = 3;

	for(x = 0; x < 800; ++x){
		y += x;
	}

	ret y;
}
