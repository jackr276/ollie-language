/**
* This program is made for the purposes of testing case statements
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;
	let mut _:u32 := 2;

	switch on(x) from(1, 16){
		case 2:
			x := 32;
			break;
		case 1:
			x := 3;
		case 33:
			x := 2;
		default:
			break;
	}


	//So it isn't optimized away
	ret x;
}
