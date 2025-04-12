/**
* SSA Testing
*/

fn other_test(mut a:i32*) -> void{
	let mut l:i32 := 33;
	let mut j:i32 := 3232;

	if(l == 32) then {
		l := l + 3222;
		j := 323;
		if(j == 323) then {
			let mut z:i32 := 32;
		}
	} else {
		l := 32;
		j := l + 3;
	}

	*a := j + 2;

	ret;
}



fn main() -> i32{
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

	x := x + 322;

	idle;

	ret x;
}
