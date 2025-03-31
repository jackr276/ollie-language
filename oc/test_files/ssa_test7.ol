/**
* SSA Testing
*/

#file SSA_TEST7;

//Global variables
let mut glob_x:u32 := 3232;
let glog_y:i32 := -232;

/*
fn tester() -> void{
	let mut x:i32 := 33;
	let mut y:i32 := 3232;

	if(x <= 32) then{
		x := x + 22;
	} else if(x == 23) then {
		x:= 323;
	} else if(x == 36) then {
		x := 32222;
	} else {
		x := 32;
	}
}
*/


fn main() -> i32{
	let mut x:i32 := 33;
	let mut y:i32 := 3232;

	x := 3222;

	if(x <= 32) then{
		x := x + 22;
	} else {
		x := x - 3;
		y := 11;

		if( x - y == 0) then{
			x := 32;
		} else {
			y := 32;
		}
	}

	idle;

	ret x;
}
