/**
* This program is made for the purposes of testing short circuiting logic
*/

fn tester(mut arg:i32) -> void{
	arg++;
}

pub fn main(arg:i32, argv:char**) -> i32{
	let mut x:u32 = 232;

	//Assign B a start
	let mut b:i32 = 33;

	for(let mut a:u32 = 0; b != 0 || a < 33; a++) {
		b--;
		x = x - 5;
	}

	//So it isn't optimized away
	ret x;
}
