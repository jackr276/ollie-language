/**
* Test loop nesting
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = -1;

	for(let _:mut u32 = 0; _ < 3333; _++) {
		for(let idx:mut u32 = 0; idx < 322; idx++) {
			x = x - 3;	
			y = y + x;
		}
	}


	ret x + y;
}
