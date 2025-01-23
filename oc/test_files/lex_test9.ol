define enum my_enum{
	ENUM_TYPE_1,
	ENUM_TYPE_2,
	ENUM_TYPE_3,
	ENUM_TYPE_4
} as custom_enum;



func main(u_int32 argc, char** argv) -> u_int32{
	while(argc < 5) do {
		declare u_int32 i;
		i := 3;
	}

	//Array of addresses
	declare u_int32*[32] address_array;

	let custom_enum mine := ENUM_TYPE_1;
	declare u_int32 j;

	for(j := 0; j <= 33; j++) do {
		j := j - 3;
	}

	ret j;
}
