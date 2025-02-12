fn my_func(u32 i, u32 j) -> i32{
	i := i + 1;
	ret i;
}


fn main(u32 argc, char** argv)->i32{
	let i32 i := 0;
	let i32 a := 0;
	let i32 v := 0;
	let i32 b := 0;
	let i32 j := 0;
	let i32 sadf := 0;

	declare char* abcd;
	let char ex := 'c';

	**argv := ~ex;
	
	/*
	while(a <= 32) do {
		if(i <= 0) then {
			a := a + 1;
		} else if( i == 1) then{
			a := 23232;
			if(a == 3232) then{
				a := a + 1;
			} else {
				a := a + 2;
			}
		} else {
			asn a := 0x23a;
		}

		//Just some junk
		let i32 sadfa := 232;

		a := a + 323;
	}
	*/

	for(let u32 i := 3; i < 232; i := i + 1) do{
		a := a + 1;
		continue when (a == 32);
		//break when(a == 32);
		let u32 masdfasdf := 232;
	}

	@my_func(i, j);

	let f32 j_copy := 32.0;
	++j_copy;

	ret j + a;
}
