func main(u_int32 argc, char& argv) -> u_int32 {
	let char x := 'a';
	let char y := 'c';

	/* Should fail on parse */
	let u_int64 x := 1;
	let u_int64 x := 2;
	
	switch on(x){
		case 1 {
			declare u_int32 i;
			asn i := 32;
			break;
		}

		default {
			break;
		}
	}
	
	ret x % y;
}
