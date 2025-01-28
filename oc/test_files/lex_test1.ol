/**
* Sample program
*/

func never_defined(float32 example) -> void;

declare u_int8 j;

alias u_int8[100] as int_arr;

func test_func(float32 my_float, void* ptr) -> void{
	let u_int32 i:= 0x01;
	//Cast ptr to an int
	asn i := *(<u_int32*>ptr);

	ret;
}

func:static main() -> u_int32* {
	let u_int32 i := 1;
	let u_int32 j := 1;
	let u_int32* j_ptr := &j;
	let u_int32* i_ptr := &i;

	ret *i_ptr + *j_ptr;
}
