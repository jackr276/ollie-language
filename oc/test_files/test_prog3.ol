
func:static main(char** argv) -> i32{
	declare u32 a;
	declare u32 b;
	declare u32 c;
	declare u32 d;

	asn a := 32;
	asn b := 27;
	asn c := 23;
	asn d := 22;

	asn a := (a+b) & (b << 2) * (c+d);

	if(a >= 3) then{
		a++;
	}

	ret a;
}
