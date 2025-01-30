
define construct my_struct {
	u_int32 a;
	s_int32 b;
	float32 c;
} as my_struct;


func test_func(u_int32 i) -> void{
	asn i := 32;
}


func main(s_int32 argc, char** argv) -> s_int32{
	//Allocate a struct
	declare my_struct my_structure;

	asn my_structure:a := 2;
	asn my_structure:b := 3;
	asn my_structure:c := 32.2;

	let s_int64 j := 2342l;

	//Sample call
	@test_func(2);

	let u_int32 idx := 0;

	while(idx < 15) do{
		@test_func(2);
		idx++;
	}
	
	asn idx := 23;

	do{
		idx--;
		@test_func(idx);

	} while (idx > 0);

	//Example for loop
	for(let u_int32 i := 0; i <= 234; asn i := i + 2) do{
		@test_func(i);
	}


	ret my_structure:b;
}
