/**
* SSA Testing
*/

//Global variables
let glob_x:mut u32 = 3232;
let glog_y:i32 = -232;

pub fn main() -> i32{
	let x:mut i32 = 33;
	let y:mut i32 = 3232;

	x = 3222;

	if(x <= 32) {
		x = x + 22;
	} else {
		x = x - 3;
		y = 11;
	}

	idle;

	x = x + 33;
	y = y - 1;


	ret x + y;
}
