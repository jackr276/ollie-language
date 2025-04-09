/**
* SSA Testing
*/

//Global variables
let mut glob_x:u32 := 3232;
let glog_y:i32 := -232;

fn other_test() -> void{
	let mut l:i32 := 33;
	let mut j:i32 := 3232;

	if(l == 32) then {
		/*
		l := l + 3222;
		j := 323;
		*/
		let mut iii:i32 := -2;
		@other_test();
	}/* else {
		l := 32;
		j := l + 3;
	}
	*/

	let mut asd:i32 := l + j;
}



fn main() -> i32{
	@other_test();

	idle;

	ret 3;
}
