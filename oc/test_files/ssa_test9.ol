/**
* SSA Testing
*/

//Global variables
fn other_test() -> void{
	let glob_x:mut u32 = 3232;
	let glog_y:i32 = -232;

	let l:mut i32 = 33;
	let j:mut i32 = 3232;

	if(l == 32) {
		l = l + 3222;
		j = 323;
		let iii:mut i32 = -2;
		@other_test();
	} else {
		l = 32;
		j = l + 3;
	}

	let asd:mut i32 = l + j;
	ret;
}



pub fn main() -> i32{
	@other_test();

	idle;

	ret 3;
}
