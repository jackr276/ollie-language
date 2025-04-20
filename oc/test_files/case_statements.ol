/**
* This program is made for the purposes of testing case statements
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;
	let mut _:u32 := 2;

	switch on(x + 1) from(1, 223){
		case 2:
			x := 32;
		case 1:
			x := 3;
		case 22222:
			x := 2;
		default:
			x := x - 22;
	}


	//So it isn't optimized away
	ret x;
}
