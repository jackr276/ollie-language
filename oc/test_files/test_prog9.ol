#file TEST_PROG_8;

fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	
	let mut a:i32 := 23;

	
	
	while(@tester(a) != 1) do{
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
	}
	
	
	/*
	while(@tester(a) != 1) do{
		a := a + 333;
		ret a;
	}
	*/
	
	

	/*
	for(let i:u32 := 0; i < 3333; ++i) do{
		@tester(i);
	}
	*/
	

	
	let i:char := 'a';

	
	if(i < 'A') then{
		ret -3333;
	} else if(i == 'A') then{
		ret -1;
	}
	
	
	

	ret 0;
}
