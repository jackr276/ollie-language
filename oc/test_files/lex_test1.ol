/**
* Sample program
*/

declare u_int8 j;

alias u_int8[100] as int_arr;

func test_func(float32 my_float, void* ptr) -> void{
	let u_int32 i := 1;
	//Cast ptr to an int
	asn i := *(<u_int32*>ptr);

	ret;
}

func:static main() -> u_int32* {
	//Single line comment
	declare int_arr j_arr;
	let s_int8 my_int := 1;
	let float32 my_float := 0.3;
	let u_int32 i := 3;
	declare float32 f;

	asn f := i;

	@test_func(my_float, i);

	declare s_int32[32] my_array;
	
	let u_int8 i := my_float || (1 && my_int);

    let u_int8* i_ptr := &i; //Another comment
	
	let u_int32 main := 1; //Illegal type redef

	let u_int8 j /*also single line comment */:= 1;
	let u_int8* j_ptr := &j;

	@test_func(i);

	ret *i + *j;
}
