/**
* This one should not work
*/

func:static example(s_int32* my_arr, u_int8 max) -> void{
	*(my_arr) := 2*3 + 6-1;
	ret;
}

func main(s_int32 argc, char** argv) -> s_int32 {
	declare s_int32[500] my_arr;
	my_arr[0] := 3;

	let u_int32 argc := 14;
	//let u_int32 example := 2;

	while(argc >= 0) do {
		argc--;
	}

	do {
		argc++;
	} while(argc < 15);
	
	@example(my_arr);

	ret 0;
}
