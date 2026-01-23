/**
* Author: Jack Robbins
* Testing for floating point branching
*/

pub fn float_compare(xx:f32, y:f32, z:f32, aa:f32) -> i32 {
	let x:mut i32 = 0;

	if(xx > y) {
		x += 3;
	}

	if (xx < y) {
		x += 55;
	}

	if (xx >= y) {
		x += 252;
	}

	if ( z <= aa) {
		x += 98;
	}

	if(xx == z) {
		x += 83;
	}

	if(z != aa){
		x += 33;
	}

	//So the optimizer doesn't blow it away
	ret x;
}


pub fn main() -> i32 {
	ret @float_compare(3.2, 32.3, .3, .4);
}
