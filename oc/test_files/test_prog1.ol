
define construct my_struct {
	u32 a;
	i32 b;
	f32 c;
} as my_struct;


fn my_func(mut u32 args, mut f32 my_float) -> u32{
	if(args == 2) then{
		ret 3;
	} else {
		args++;
	}

	defer args++;
	defer args--;
	
	if(args == 0) then {
		ret args;
	} else if(args > 0) then {
		args++;
	} else {
		my_float := 32.2;
		//ret *(<u32*>(&my_float));
	}

	for(let mut u32 i := 0; i < 232; i++) do{
		i--;
		let i32 j := 32;
		continue when (i == 32);
	}

	ret args;
}


fn test_func(mut u32 i) -> void{
	i := 32;
}


fn main(i32 argc, char** argv) -> i32{
	//Allocate a struct
	declare my_struct my_structure;

	my_structure:a := 2;
	my_structure:b := 3;
	 my_structure:c := 32.2;

	let i64 j := 2342l;

	//Sample call
	@test_func(2);

	let mut u32 idx := 0;

	while(idx < 15) do{
		let u32 bab := @my_func(idx, 32.2);
		idx++;
	}
	
	idx := 23;

	do{
		idx--;
		@test_func(idx);

		if(idx == 12) then {
			ret idx;
			idx := 23;
		}

	} while (idx > 0);

	//Example for loop
	for(let mut u32 i := 0; i <= 234; i := i + 2) do{
		@test_func(i);
	}


	ret my_structure:b;

	let u32 my_integer := 0x02;
}
