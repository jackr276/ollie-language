/**
* This program is made for the purposes of testing conditional breaks
*/

pub fn main(arg:i32, argv:char**) -> i32{
	let x:mut i32 = 32;

	let _:mut u32 = 2;

	while (_ < 65) {
		let i:mut u32 = 2;
		if(i == 2) {
			i = 3;
		}
		break when(x > 33 || x <= 55);
		x = x * 37;
		_++;
	}

	//So it isn't optimized away
	ret x;
}
