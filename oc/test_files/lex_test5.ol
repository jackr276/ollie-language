

fn main(argc:u32, argv:char**) -> i32 {
	let x : char := 'a';
	let y : char := 'c';

	let mut x:u64 := 1;
	
	switch on(x){
		case 1 -> {
			declare i : u32;
			i := 32;
		}

		default -> {
			x := 222;
		}
	}
	
	ret x % y;
}
