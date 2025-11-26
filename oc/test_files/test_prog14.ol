/**
 * Author: Jack Robbins
 * Block merging test
*/

fn tester(a:u32) -> u32 {
	ret a;
}

pub fn main(argc:i32, argv:char**) -> i32 {
	let a:mut u32 = 32;

    for(let _:mut u32 = 0; _ < 23; _++){
		@tester(a--);
	}

	ret a;
}
