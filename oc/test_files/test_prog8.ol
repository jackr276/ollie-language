#file TEST_PROG_8;

fn tester() -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	let a:i32 := 23;
	declare mut x:i32*;
	//We would've modified a here

	for(let _:u32 := 0; _ < 23; _++) do{
		a++;
	}

	switch on(@tester()){
		case 2:
			let a:i32 := 23;
			idle;
		case 4:
			idle;
		default:
			idle;
	}

	ret 0;
}
