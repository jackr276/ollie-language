fn main(u32 argc, char** argv) -> i32 {
	let char x := 'a';
	let char y := 'c';

	/* Should fail on parse */
	let u64 x := 1;
	
	switch on(x){
		case 1:
			declare u_int32 i;
			i := 32;
			break;

		default:
			break;
	}
	
	ret x % y;
}
