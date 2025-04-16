/**
* This program is made for the purposes of testing conditional breaks
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;

	for(let _:u32 := 0; _ < 32; _++) do{
		x := x * 37;
		break when(_ == 2);
	}

	//So it isn't optimized away
	ret x;
}
