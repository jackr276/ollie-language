/**
* Author: Jack Robbins
* Test our ability to raise errors even if RDX is used
*/

pub fn! raise_with_rdx(x:i32, y:i32, z:i32) -> i32 {
	if (x < 0) {
		raise error;
	}

	if(z > 0) {
		raise error;
	}

	ret x + y - z;
}


pub fn main() -> i32 {
	ret 0;
}
