/**
* SSA Testing
*/

pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut u32 = argc;
	defer {
		x++;
	};

	if( x == 3222){
		ret x;
	}
	

	x = 323;
	ret 0;
}
