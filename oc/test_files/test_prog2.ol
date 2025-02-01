let u_int32 i := 0;

declare u_int32 j;



func test_func() -> s_int32 {
	let u_int32 i := 232;

	if(i == 2) then{
		asn i := i + 2;
		ret i;
	} else {
		asn i := i + 3;
		ret i;
	}

	let s_int32 my_int := -2;
	ret 32;
}



/*
func main(u_int32 argc, char** argv)->s_int32{
	let u_int32 i := 0;
	let u_int32 a := 0;
	let u_int32 v := 0;
	let u_int32 b := 0;
	let u_int32 sadf := 0;


	declare u_int32 j;

	ret j + a;
}
*/
