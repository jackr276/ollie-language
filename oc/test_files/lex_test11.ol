/**
* This one should not work
*/

fn example(my_arr:mut i32*, max:u32) -> void{
	*my_arr = 2+3 + 6-1;
	ret;
}

pub fn main(argc:i32, argv:char**) -> i32 {
	declare my_arr:mut i32[400];
	my_arr[0] = 3;

	let argc:mut u32 = 14;

	while(argc > 0) {
		argc--;
	}

	do {
		argc++;
	} while(argc < 15);
	
	@example(my_arr, 23);

	ret 0;
}
