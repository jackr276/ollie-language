func main(u_int32 argc, char** argv) -> u_int32 {
	let char x := 'a';
	let char y := 'c';

	/* Should fail on parse */
	let u_int64 x := 1;
	let u_int64 y := 2;
	
	switch on(x){
		case 1:
			break;

		default:
			break;
	}
	
	ret x % y;
}

defined structure my_type {
	u_int32 sample;
};


