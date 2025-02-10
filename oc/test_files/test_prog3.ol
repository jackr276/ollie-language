fn my_fn() -> void{
	let u32 i := 32;
}

fn:static main(char** argv) -> i32{
	declare u32 a;
	declare u32 b;
	declare u32 c;
	declare u32 d;

	asn a := 32;
	asn b := 27;
	asn c := 23;
	asn d := 22;

	asn a := (a+b) & (b << 2) * (c+d);


	if(a >= 2) then{
		asn a := a + 3;
	} else if (a == 2) then{
		asn a := a + 2;
	} else {
		asn a := a + 1;
	}


	//do{
	//while(d >= 32) do{
	for(let u32 i := 0; i < 323; i++) do{
		declare u32 a;
		declare u32 b;
		declare u32 c;

		asn a := 32;
		asn b := 27;
		asn c := 23;

	};// while(d >= 32);

	@my_fn();
	let u32 abcd := 322;

	ret a;
}
