/**
* Example
*/

fn my_func(i:mut u32, j:mut u32) -> i32{
	i = i + 1;
	ret i;
}


fn my_fn(argc:mut i32, argv:mut char**)->i32{
	let i:mut i32 = 0;
	let a:mut i32 = 0;
	let v:i32 = 0;
	let b:i32 = 0;
	let j:i32 = 0;
	let sadf:i32 = 0;

	declare abcd:char*;
	let ex:char = 'c';

	**argv = ~ex;

	--argv;
	
	
	while(a <= 32) {
		idle;
		idle;
		if(i <= 0)  {
			a = a + 1;
		} else if( i == 1) {
			a = 23232;
			if(a == 3232) {
				a = a + 1;
			} else {
				a = a + 2;
			}
		} else {
			a = 0x23a;
		}

		//Just some junk
		let sadfa:i32 = 232;

		a = a + 323;
	}
	

	@my_func(i+2, j);

	let j_copy:mut u32 = -i * 32 - 322;
	++j_copy;

	ret j + a;
}
