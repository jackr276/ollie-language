/**
* Test loop nesting
*/

fn main(argv:char**, argc:i32) -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := -1;

	for(let _:u32 := 0; _ < 3333; _++) do{
		for(let idx:u32 := 0; idx < 322; idx++) do{
			x := x - 3;	
			y := y + x;
		}
	}


	ret x + y;
}
