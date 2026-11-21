/**
* SSA Testing
*/

pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut i32 = 3222;

	x = argc;

	if(x <= 32) {
		if(x == 70) {
			x = x + -1;
		}
	} else if(x == 77) 	{
		ret x - 4;
	} else if(x == 3232323) {
		ret x + 4555;
	} else {
		//if(x > 32) {
			x = x - 22222222;
		//}
	}

	x = -1;

	ret 0;
}
