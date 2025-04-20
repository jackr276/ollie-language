/**
* This program is made for the purposes of testing case statements
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;

	switch on(x + 1){
		case 2:
			x := 32;
		case 1:
			x := -3;
		case 3:
			x := 211;
		case 10:
			x := 22;
		default:
			x := x - 22;
	}


	//So it isn't optimized away
	ret x;
}
