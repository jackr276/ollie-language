#file TEST_PROG_8;

fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	
	let a:i32 := 23;


	
	{
	 let a:i32 := 42;
	}


	//TODO THIS MUST BE FIXED-- added in to make the code work
	idle;

	
	switch on(@tester(3)){
		case 2:
			//Test this out
			for(let _:i32 := 0; _ < 32; _++) do{
				@tester(_);
				break when (_ == 3);
			}

			let a:i32 := 23;
			idle;

			break when(a == 3);

		case 4:
			a++;
			idle;
			break;
		default:
			idle;
			break;
	}

	ret 0;
}
