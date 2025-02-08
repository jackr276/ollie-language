/**
* Sample program
*/

fn never_defined(f64 example) -> u32;

declare u8 j;

alias u8[100] as int_arr;

fn test_func(f32 my_float, void* ptr) -> void{
	let u32 i:= 0x01;
	//Cast ptr to an int
	asn i := *(<u32*>ptr);

	declare i32[200] my_arr;

	asn my_arr[3] := 2.23;

	let void my := ptr;

	//Example call
	asn i := @never_defined(3.2222);

	ret;
}

fn never_defined(f64 example) -> i32{
	ret 2;
}

fn:static main() -> i32 {
	let u32 i := 1;
	let u32 j := 1;
	let u32* j_ptr := &j;
	let u32* i_ptr := &i;

	//Example call
	asn i := @never_defined(2.32);

	ret *i_ptr + *j_ptr;
}
