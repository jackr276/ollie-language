/**
* Example
*/

#dependencies
require "./oc/test_files/test_prog3.ol";
#dependencies

fn my_func(mut i:u32, mut j:u32) -> i32{
	i := i + 1;
	ret i;
}


fn my_fn(mut argc:u32, mut argv:char**)->i32{
	let mut i:i32 := 0;
	let mut a:i32 := 0;
	let v:i32 := 0;
	let b:i32 := 0;
	let j:i32 := 0;
	let sadf:i32 := 0;

	declare abcd:char*;
	let ex:char := 'c';

	**argv := ~ex;

	--argv;
	
	/*
	while(a <= 32) do {
		idle;
		idle;
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
		let sadfa:i32 := 232;

		a := a + 323;
	}
	*/

	for(let mut i:u32 := 3; i < 232; i := i + 1) do{
		idle;
		a := a + 1;
		continue when (a == 32);
		//break when(a == 32);
		let masdfasdf:u32 := 232;
	}

	@my_func(i+2, j);

	let j_copy:u32 := -i * 32 - 322;
	++j_copy;

	ret j + a;
}
