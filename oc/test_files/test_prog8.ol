#file TEST_PROG_8;

fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	let a:i32 := 23;
	declare mut x:i32*;
	//We would've modified a here

	//x := &a;

	for(let _:u32 := 0; _ < 23; _++) do{
		@tester(a--);
	}

	//TODO THIS MUST BE FIXED-- added in to make the code work
	idle;

	switch on(@tester(a)){
		case 2:
			let a:i32 := 23;
			idle;
		case 4:
			a++;
			idle;
		default:
			idle;
	}

	ret 0;
}
