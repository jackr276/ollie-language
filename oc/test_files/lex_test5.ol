

fn main(argc:u32, argv:char**) -> i32 {
	let x : char := 'a';
	let y : char := 'c';

	let mut x:u64 := 1;
	
	switch on(x){
		case 1:
			declare i : u32;
			i := 32;
			break;

		default:
			break;
	}
	
	ret x % y;
}
