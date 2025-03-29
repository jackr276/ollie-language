/**
* SSA Testing
*/

#file SSA_TEST6;

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

	x := -1;

	ret y;
}
