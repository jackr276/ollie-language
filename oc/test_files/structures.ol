/**
* This program is made for the purposes of testing structures
*/

pub fn main(arg:i32, argv:char**) -> i32{
	define struct s {
		x:i32;
		y:i32;
	} as my_struct;

	let x:mut i32 = 32;
	let _:mut u32 = 2;

	switch(x){
		case 2 -> {
			x = 32;
		}
		case 1 -> {
			x = 3;
		}
		case 33 -> {
			x = 2;
		}
		// Empty default
		default -> {}
	}


	//So it isn't optimized away
	ret x;
}
