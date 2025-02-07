
define construct my_struct {
	u32 a;
	i32 b;
	f32 c;
} as my_struct;


func my_func(u32 args, f32 my_float) -> u32{
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
		asn my_float := 32.2;
		ret *(<u32*>(&my_float));
	}

	for(let u32 i := 0; i < 232; i++) do{
		i--;
		let i32 j := 32;
		continue when (i == 32);
	}

	ret args;
}


func test_func(u32 i) -> void{
	asn i := 32;
}


func main(i32 argc, char** argv) -> i32{
	//Allocate a struct
	declare my_struct my_structure;

	asn my_structure:a := 2;
	asn my_structure:b := 3;
	asn my_structure:c := 32.2;

	let i64 j := 2342l;

	//Sample call
	@test_func(2);

	let u32 idx := 0;

	while(idx < 15) do{
		let u32 bab := @my_func(idx, 32.2);
		idx++;
	}
	
	asn idx := 23;

	do{
		idx--;
		@test_func(idx);

		if(idx == 12) then {
			ret idx;
			asn idx := 23;
		}

	} while (idx > 0);

	//Example for loop
	for(let u32 i := 0; i <= 234; asn i := i + 2) do{
		@test_func(i);
	}


	ret my_structure:b;

	let u32 my_integer := 0x02;
}
