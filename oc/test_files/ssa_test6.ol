/**
* SSA Testing
*/

//Global variables
let mut glob_x:u32 := 3232;
let glog_y:i32 := -232;

fn main() -> i32{
	let mut x:i32 := 33;
	let mut y:i32 := 3232;

	x := 3222;

	if(x <= 32) then{
		x := x + 22;
	} else {
		x := x - 3;
		y := 11;
	}

	idle;

	//while(x <= 322) do{
	//do{
//	for(let _:u32 := 0; _ <= 323; _++) do{
		x := x + 33;
		y := y - 1;

	//	break when(x == 32);
	}//while(x <= 322);


	ret x + y;
}
