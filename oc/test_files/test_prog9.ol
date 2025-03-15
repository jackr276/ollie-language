#file TEST_PROG_8;

fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	
	let a:i32 := 23;

	if(a == 32) then{
		a++;
	} else if(a == 33) then{
		a--;
	}

	ret 0;
}
