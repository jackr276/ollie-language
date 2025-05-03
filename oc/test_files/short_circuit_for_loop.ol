/**
* This program is made for the purposes of testing short circuiting logic
*/

fn tester(arg:i32) -> void{
	arg++;
}

fn main(arg:i32, argv:char**) -> i32{
	let mut x:u32 := 232;

	//Assign B a start
	let mut b:i32 := 33;

	for(let a:u32 := 0; b != 0 || a < 33; a++) do{
		b--;
		x := x - 5;
	}

	//So it isn't optimized away
	ret x;
}
