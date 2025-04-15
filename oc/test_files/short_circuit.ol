/**
* This program is made for the purposes of testing short circuiting logic
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:u32 := 232;

	if(x >= 3 && x <= 32) then{
		x := x - 3;
	} else if(x < 2 && (x != 1 || x != 2)) then {
		x := x * 2;
	} else {
	 	x := x + 3;
	}

	//So it isn't optimized away
	ret x;
}
