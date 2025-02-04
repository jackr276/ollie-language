func test_func() -> i32 {
	let u32 i := 232;
	let u32 j := 32;

	while(i >= 232) do{
		if(i == 2) then{ 
			asn i := i + 2;
			ret i;
		} else if(i == 3) then {
			asn i := 32;
			//ret i;
		} else {
			asn i := i + 1;
			ret i;
		}
	}

	defer i++;

	while(i >= 0) do{
		i--;

		break when(i < 0);
		//ret i;
	}

	do {
		i++;
		@test_func();
		if(i < 3) then {
			continue;
		} else {
			i++;
		}

		ret i;
	} while (i < 232);

	for(let u32 i := 0; i < 2323; i++) do{
		@test_func();
		if(i == -1) then{
			ret -1;
		}

	}

	let i32 my_int := -2;
	ret 32;
}




func main(u32 argc, char** argv)->i32{
	let u32 i := 0;
	let u32 a := 0;
	let u32 v := 0;
	let u32 b := 0;
	let u32 j := 0;
	let u32 sadf := 0;

	if(i == 0) then {
		if(j == a) then{
			asn a := 3;	
		} else if (j > a) then {
			ret j;
		}
	} else if( i == 1) then{
		let u32 i_copy := i;
	} else {
		@test_func();
		let u32 j_copy := i;
	}

	defer @test_func();
	defer @test_func();

	ret j + a;
}

