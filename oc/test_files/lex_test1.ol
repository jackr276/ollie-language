/**
* Sample program
*/

func never_defined(float64 example) -> u_int32;

declare u_int8 j;

alias u_int8[100] as int_arr;

func test_func(float32 my_float, void* ptr) -> void{
	let u_int32 i:= 0x01;
	//Cast ptr to an int
	asn i := *(<u_int32*>ptr);

	declare s_int32[200] my_arr;

	asn my_arr[3.2] := 2;

	let void my := ptr;

	//Example call
	asn i := @never_defined(3.2222);

	ret;
}

func never_defined(float64 example) -> u_int32{
	ret 2;
}

func:static main() -> s_int32 {
	let u_int32 i := 1;
	let u_int32 j := 1;
	let u_int32* j_ptr := &j;
	let u_int32* i_ptr := &i;

	//Example call
	asn i := @never_defined(2.32);

	ret *i_ptr + *j_ptr;
}
