

/*
fn my_fn() -> void{
	let i:u32 = 32;
}
*/

fn test(argc:i32, argv:char**) -> i32{
	declare mut a:u32;
	declare mut b:u32;
	declare mut c:u32;
	declare mut d:u32;

	a = 32;
	b = 27;
	c = 23;
	d = 22;

	a = (a+b) & (b << 2) * (c+d);


	if(a >= 2) {
		a = a + 3;
	} else if (a == 2) {
		a = a + 2;
	} else {
		a = a + 1;
	}


	//do{
	//while(d >= 32) {
	for(let mut i:u32 = 0; i < 323; i++) {
		declare a:u32;
		declare b:u32;
		declare c:u32;

		a = 32;
		b = 27;
		c = 23;

	};// while(d >= 32);

	let abcd:u32 = 322;

	ret a;
}
