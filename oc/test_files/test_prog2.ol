
#file TEST_PROG_2;

fn test_func() -> i32 {
	let mut i:u32 := 232;
	let mut j:u32 := 32;

	defer i++;

	while(i >= 232) do{
		if(i == 2) then{ 
			i := i + 2;
			ret i;
		} else if(i == 3) then {
			i := 32;
			//ret i;
		} else {
			i := i + 1;
			ret i;
		}
	}

	defer i++;

	while(i >= 0) do{
		i--;

		//break when(i < 0);
		ret i;
	}

	do {
		i++;
		@test_func();

		//ret i;
	} while (i < 232);

	let sample:u32 := 2232;
	for(let i:u32 := 0; i < 2323; i++) do {
		@test_func();
		if(i == -1) then{
			ret -1;
		}

	}

	let my_int:i32 := -2;
	ret 32;
}

fn main(argc:u32, argv:char**)->i32{
	let i:u32 := 0;
	let mut a:u32 := 0;
	let v:u32 := 0;
	let b:u32 := 0;
	let j:u32 := 0;
	let sadf:u32 := 0;

	if(i == 0) then {
		if(j == a) then{
			a := 3;	
		} else if (j > a) then {
			ret j;
		}
	} else if( i == 1) then{
		let i_copy:u32 := i;
	} else {
		@test_func();
		let j_copy:u32 := i;
	}

	defer @test_func();
	defer @test_func();

	ret j + a;
}

