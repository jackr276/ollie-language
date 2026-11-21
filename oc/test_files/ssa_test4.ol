/**
* SSA Testing
*/

pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut u32 = 32;

	x = argc;

	for(let i:mut u32 = 3; i <= 323; ++i){
		x = i - 15;
		break when( x == 5);
	}

	ret 0;
}
