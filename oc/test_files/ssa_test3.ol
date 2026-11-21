/**
* SSA Testing
*/

pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut u32 = argc;

	if( x == 3222) {
		ret x;
	} else if (x == 11) {
		x = x + 33;
	} else {
		x--;
	}

	ret 0;
}
