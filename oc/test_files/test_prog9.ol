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
		if(a > 32) then{
			let i:i32 := 3222;
		} else if(a == -3) then{
			let i:i32 := -2322;
		}
	} else if(a==3222) then{
		if(a > 45) then{
			let i:i32 := 3222;
		} else {
			let i:i32 := -2322;
		}
	}

	let i:char := 'a';

	if(i < 'A') then{
		ret -2;
	} else {
		ret 33;
	}

	ret 0;
}
