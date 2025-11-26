
define struct my_struct {
	a:mut u32;
	b:mut u32;
	c:mut u32;
} as my_struct;


fn my_func(args:mut u32) -> u32{
	if(args == 2) {
		ret 3;
	} else {
		args++;
	}

	
	if(args == 0)  {
		ret args;
	} else if(args > 0)  {
		args++;
	} 

	for(let i:mut u32 = 0; i < 232; i++){
		i--;
		let j:i32 = 32;
		continue when (i == 32);
	}

	ret args;
}


fn test_func(i:mut u32) -> void{
	i = 32;
}


pub fn main(argc:i32, argv:char**) -> i32{
	//Allocate a struct
	declare my_structure:mut my_struct;

	my_structure:a = 2;
	my_structure:b = 3;
	my_structure:c = 32;

	let j:i64 = 2342l;

	//Sample call
	@test_func(2);

	let idx:mut u32 = 0;

	while(idx < 15){
		let bab:u32 = @my_func(idx);
		idx++;
	}
	
	idx = 23;

	do{
		idx--;
		@test_func(idx);

		if(idx == 12)  {
			ret idx;
			idx = 23;
		}

	} while (idx > 0);

	//Example for loop
	for(let i:mut u32 = 0; i <= 234; i = i + 2){
		@test_func(i);
	}


	ret my_structure:b;
}
