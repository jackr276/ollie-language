/**
* SSA Testing
*/

//Global variables
fn other_test(glob_x:mut u32, glog_y:i32, l:mut i32, j:mut i32) -> void{
	if(l == 32) {
		l = l + 3222;
		j = 323;
		let iii:mut i32 = -2;
		@other_test(1, 2, 3, 4);
	} else {
		l = 32;
		j = l + 3;
	}

	let asd:mut i32 = l + j;
	ret;
}



pub fn main() -> i32{
	@other_test(1, 2, 3, 4);

	idle;

	ret 3;
}
