/**
* This program is made for the purposes of testing conditional breaks
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;

	let mut _:u32 := 2;

	//for(let _:u32 := 0; _ < 32; _++) do{
	while (_ < 33) do {
		continue when(x == 3222);
		x := x * 37;
		_++;
	}

	//So it isn't optimized away
	ret x;
}
