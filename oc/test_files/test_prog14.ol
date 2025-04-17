/**
 * Author: Jack Robbins
 * Block merging test
*/

fn tester(a:u32) -> u32 {
	ret a;
}

fn main(argc:u32, argv:char**) -> i32 {
	let mut a:u32 := 32;

    for(let _:u32 := 0; _ < 23; _++) do{
		@tester(a--);
	}

	ret a;
}
