
defined structure my_type {
	u_int32 sample;
};


func main(u_int32 argc, str* argv) -> u_int32 {
	let char x = 'a';
	let char y = 'c';

	/* Should fail on parse */
	let u_int64 x = 1;
	let u_int64 y = 2;
	let u_int64 z = size(defined structure my_type*);
	
	switch on(x){
		case 1:
			ret x + y;
			break;

		default:
			break;
	}
	
	ret x % y;
}
