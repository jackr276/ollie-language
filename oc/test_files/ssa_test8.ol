/**
* SSA Testing
*/

#file SSA_TEST8;

fn main() -> i32{
	let mut x:i32 := 33;
	let mut y:i32 := 3232;

	if(x == 32) then {
		x := x + 3222;
		y := 323;
	} else {
		x := 32;
		y := x + y + 3;
	}

	let mut w:i32 := x + y;

	w := 327;
	w := 322;

	idle;

	ret w;
}
