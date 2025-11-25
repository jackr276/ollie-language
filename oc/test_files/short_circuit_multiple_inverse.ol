/**
* This program is made for the purposes of testing short circuiting logic
*/

fn tester(arg:mut i32) -> void{
	arg++;
}

pub fn main(arg:i32, argv:char**) -> i32{
	let x:mut u32 = 232;

	//Assign B a start
	let b:mut i32 = 33;

	let a:mut u32 = 0;

	//These are both inverses
	while(a < 33 && (b == 0 || b - a == 0)){
		b--;
		x = x - 5;
	}

	//So it isn't optimized away
	ret x;
}
