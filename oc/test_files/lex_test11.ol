/**
* This one should not work
*/

fn:static example(i32* my_arr, u8 max) -> void{
	asn *(my_arr) := 2+3 + 6-1;
	ret;
}

fn main(i32 argc, char** argv) -> i32 {
	declare i32[500] my_arr;
	asn my_arr[0] := 3;

	let u32 argc := 14;
	//let u32 example := 2;

	while(argc > 0) do {
		argc--;
	}

	do {
		argc++;
	} while(argc < 15);
	
	@example(my_arr);

	ret 0;
}
