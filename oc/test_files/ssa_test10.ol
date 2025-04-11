/**
* SSA Testing
*/

fn other_test(mut a:i32*) -> void{
	let mut l:i32 := 33;
	let mut j:i32 := 3232;

	if(l == 32) then {
		l := l + 3222;
		j := 323;
	} else {
		l := 32;
		j := l + 3;
	}

	*a := j + 2;

	ret;
}



fn main() -> i32{

	idle;

	ret 3;
}
