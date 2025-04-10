/**
* SSA Testing
*/


fn main() -> i32{
	let mut x:i32 := 33;
	let mut y:i32 := 3232;

	let mut abc:i32 := 3232;
	//100% useless
	if(abc == 1) then {
		abc := 2;
	} else {
		abc := 3;
	}

	if(x == 32) then {
		x := x + 3222;
		let z:i32 := -2;
		y := 323;
		let ab:i32 := 232323;
	} else {
		x := 32;
		y := x + y + 3;
	}

	let mut w:i32 := x + y;

//	w := 327;
//	w := 322;

	idle;

	ret w;
}
