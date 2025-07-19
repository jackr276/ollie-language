/**
* This program is made for the purposes of testing structures
*/

fn main(arg:i32, argv:char**) -> i32{
	define construct s {
		x:i32;
		y:i32;
	} as my_struct;

	let mut x:i32 := 32;
	let mut _:u32 := 2;

	switch(x){
		case 2 -> {
			x := 32;
		}
		case 1 -> {
			x := 3;
		}
		case 33 -> {
			x := 2;
		}
		// Empty default
		default -> {}
	}


	//So it isn't optimized away
	ret x;
}
