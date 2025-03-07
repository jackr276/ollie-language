#file TEST_PROG_8;

fn tester() -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{

	let mut x:i32 := -2U;

	switch on(@tester()){
		case 2:
			let a:i32 := 23;
			idle;
		case 4:
			idle;
		default:
			{
			x := x + 3;
			}
	}

	ret 0;
}
