/**
* SSA Testing
*/

#file SSA_TEST6;

fn main() -> i32{
	let mut x:i32 := 33;

	x := 3222;

	if(x <= 32) then{
		x := x + 22;
	} else {
		x := x - 3;
	}

	x := -1;

	ret 0;
}
