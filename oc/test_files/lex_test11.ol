/**
* This one should work
*/


func:static example(s_int32* my_arr, u_int8 max) -> void{
	asn *(my_arr) := 2;
	ret;
}



func main(s_int32 argc, char** argv) -> s_int32 {
	declare s_int32 my_arr[500];
	asn my_arr[0] := 3;

	let u_int32 my_int := 14;
	
	example(my_arr);

	ret 0;
}
