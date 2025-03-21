/**
 * Author: Jack Robbins
 * Block merging test
*/

#file test_prog14;

fn tester(a:u32) -> u32 {
	ret a;
}

fn main(argc:u32, argv:char**) -> i32 {
	let mut a:u32 := 32;

    for(let _:u32 := 0; _ < 23; _++) do{
		@tester(a--);
	}

	switch on(@tester(a)){
		case 2:
			let a:i32 := 23;
			idle;
		case 4:
			a++;
			idle;
			break when(a == 32);
		default:
			idle;
	}

	ret a;
}
