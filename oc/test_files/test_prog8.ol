#file TEST_PROG_8;

fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	let a:i32 := 23;
	declare x:i32*;
	//We would've modified a here

	x := &a;

	//This should work
	let test_thing:i32 := x[2];

	//Can we get a pointer to an array?
	declare mut x_arr:i32[32];
	x_arr[2] := 3;
	x_arr[3] := 3;
	x_arr[4] := 3;

	let arr_base_ptr:i32* := x_arr;

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
